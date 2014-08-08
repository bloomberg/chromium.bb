// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/history_quick_provider.h"

#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/in_memory_url_index.h"
#include "chrome/browser/history/in_memory_url_index_types.h"
#include "chrome/browser/history/scored_history_match.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/omnibox/autocomplete_match_type.h"
#include "components/omnibox/omnibox_field_trial.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/url_parse.h"
#include "url/url_util.h"

using history::InMemoryURLIndex;
using history::ScoredHistoryMatch;
using history::ScoredHistoryMatches;

bool HistoryQuickProvider::disabled_ = false;

HistoryQuickProvider::HistoryQuickProvider(Profile* profile)
    : HistoryProvider(profile, AutocompleteProvider::TYPE_HISTORY_QUICK),
      languages_(profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)) {
}

void HistoryQuickProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  matches_.clear();
  if (disabled_)
    return;

  // Don't bother with INVALID and FORCED_QUERY.
  if ((input.type() == metrics::OmniboxInputType::INVALID) ||
      (input.type() == metrics::OmniboxInputType::FORCED_QUERY))
    return;

  autocomplete_input_ = input;

  // TODO(pkasting): We should just block here until this loads.  Any time
  // someone unloads the history backend, we'll get inconsistent inline
  // autocomplete behavior here.
  if (GetIndex()) {
    base::TimeTicks start_time = base::TimeTicks::Now();
    DoAutocomplete();
    if (input.text().length() < 6) {
      base::TimeTicks end_time = base::TimeTicks::Now();
      std::string name = "HistoryQuickProvider.QueryIndexTime." +
          base::IntToString(input.text().length());
      base::HistogramBase* counter = base::Histogram::FactoryGet(
          name, 1, 1000, 50, base::Histogram::kUmaTargetedHistogramFlag);
      counter->Add(static_cast<int>((end_time - start_time).InMilliseconds()));
    }
  }
}

HistoryQuickProvider::~HistoryQuickProvider() {}

void HistoryQuickProvider::DoAutocomplete() {
  // Get the matching URLs from the DB.
  ScoredHistoryMatches matches = GetIndex()->HistoryItemsForTerms(
      autocomplete_input_.text(),
      autocomplete_input_.cursor_position(),
      AutocompleteProvider::kMaxMatches);
  if (matches.empty())
    return;

  // Figure out if HistoryURL provider has a URL-what-you-typed match
  // that ought to go first and what its score will be.
  bool will_have_url_what_you_typed_match_first = false;
  int url_what_you_typed_match_score = -1;  // undefined
  // These are necessary (but not sufficient) conditions for the omnibox
  // input to be a URL-what-you-typed match.  The username test checks that
  // either the username does not exist (a regular URL such as http://site/)
  // or, if the username exists (http://user@site/), there must be either
  // a password or a port.  Together these exclude pure username@site
  // inputs because these are likely to be an e-mail address.  HistoryURL
  // provider won't promote the URL-what-you-typed match to first
  // for these inputs.
  const bool can_have_url_what_you_typed_match_first =
      (autocomplete_input_.type() != metrics::OmniboxInputType::QUERY) &&
      (!autocomplete_input_.parts().username.is_nonempty() ||
       autocomplete_input_.parts().password.is_nonempty() ||
       autocomplete_input_.parts().path.is_nonempty());
  if (can_have_url_what_you_typed_match_first) {
    HistoryService* const history_service =
        HistoryServiceFactory::GetForProfile(profile_,
                                             Profile::EXPLICIT_ACCESS);
    // We expect HistoryService to be available.  In case it's not,
    // (e.g., due to Profile corruption) we let HistoryQuick provider
    // completions (which may be available because it's a different
    // data structure) compete with the URL-what-you-typed match as
    // normal.
    if (history_service) {
      history::URLDatabase* url_db = history_service->InMemoryDatabase();
      // url_db can be NULL if it hasn't finished initializing (or
      // failed to to initialize).  In this case, we let HistoryQuick
      // provider completions compete with the URL-what-you-typed
      // match as normal.
      if (url_db) {
        const std::string host(base::UTF16ToUTF8(
            autocomplete_input_.text().substr(
                autocomplete_input_.parts().host.begin,
                autocomplete_input_.parts().host.len)));
        // We want to put the URL-what-you-typed match first if either
        // * the user visited the URL before (intranet or internet).
        // * it's a URL on a host that user visited before and this
        //   is the root path of the host.  (If the user types some
        //   of a path--more than a simple "/"--we let autocomplete compete
        //   normally with the URL-what-you-typed match.)
        // TODO(mpearson): Remove this hacky code and simply score URL-what-
        // you-typed in some sane way relative to possible completions:
        // URL-what-you-typed should get some sort of a boost relative
        // to completions, but completions should naturally win if
        // they're a lot more popular.  In this process, if the input
        // is a bare intranet hostname that has been visited before, we
        // may want to enforce that the only completions that can outscore
        // the URL-what-you-typed match are on the same host (i.e., aren't
        // from a longer internet hostname for which the omnibox input is
        // a prefix).
        if (url_db->GetRowForURL(
            autocomplete_input_.canonicalized_url(), NULL) != 0) {
          // We visited this URL before.
          will_have_url_what_you_typed_match_first = true;
          // HistoryURLProvider gives visited what-you-typed URLs a high score.
          url_what_you_typed_match_score =
              HistoryURLProvider::kScoreForBestInlineableResult;
        } else if (url_db->IsTypedHost(host) &&
             (!autocomplete_input_.parts().path.is_nonempty() ||
              ((autocomplete_input_.parts().path.len == 1) &&
               (autocomplete_input_.text()[
                   autocomplete_input_.parts().path.begin] == '/'))) &&
             !autocomplete_input_.parts().query.is_nonempty() &&
             !autocomplete_input_.parts().ref.is_nonempty()) {
          // Not visited, but we've seen the host before.
          will_have_url_what_you_typed_match_first = true;
          const size_t registry_length =
              net::registry_controlled_domains::GetRegistryLength(
                  host,
                  net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
                  net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
          if (registry_length == 0) {
            // Known intranet hosts get one score.
            url_what_you_typed_match_score =
                HistoryURLProvider::kScoreForUnvisitedIntranetResult;
          } else {
            // Known internet hosts get another.
            url_what_you_typed_match_score =
                HistoryURLProvider::kScoreForWhatYouTypedResult;
          }
        }
      }
    }
  }

  // Loop over every result and add it to matches_.  In the process,
  // guarantee that scores are decreasing.  |max_match_score| keeps
  // track of the highest score we can assign to any later results we
  // see.  Also, reduce |max_match_score| if we think there will be
  // a URL-what-you-typed match.  (We want URL-what-you-typed matches for
  // visited URLs to beat out any longer URLs, no matter how frequently
  // they're visited.)  The strength of this reduction depends on the
  // likely score for the URL-what-you-typed result.

  // |template_url_service| or |template_url| can be NULL in unit tests.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  TemplateURL* template_url = template_url_service ?
      template_url_service->GetDefaultSearchProvider() : NULL;
  int max_match_score = matches.begin()->raw_score();
  if (will_have_url_what_you_typed_match_first) {
    max_match_score = std::min(max_match_score,
        url_what_you_typed_match_score - 1);
  }
  for (ScoredHistoryMatches::const_iterator match_iter = matches.begin();
       match_iter != matches.end(); ++match_iter) {
    const ScoredHistoryMatch& history_match(*match_iter);
    // Culls results corresponding to queries from the default search engine.
    // These are low-quality, difficult-to-understand matches for users, and the
    // SearchProvider should surface past queries in a better way anyway.
    if (!template_url ||
        !template_url->IsSearchURL(history_match.url_info.url(),
                                   template_url_service->search_terms_data())) {
      // Set max_match_score to the score we'll assign this result:
      max_match_score = std::min(max_match_score, history_match.raw_score());
      matches_.push_back(QuickMatchToACMatch(history_match, max_match_score));
      // Mark this max_match_score as being used:
      max_match_score--;
    }
  }
}

AutocompleteMatch HistoryQuickProvider::QuickMatchToACMatch(
    const ScoredHistoryMatch& history_match,
    int score) {
  const history::URLRow& info = history_match.url_info;
  AutocompleteMatch match(
      this, score, !!info.visit_count(),
      history_match.url_matches().empty() ?
          AutocompleteMatchType::HISTORY_TITLE :
          AutocompleteMatchType::HISTORY_URL);
  match.typed_count = info.typed_count();
  match.destination_url = info.url();
  DCHECK(match.destination_url.is_valid());

  // Format the URL autocomplete presentation.
  const net::FormatUrlTypes format_types = net::kFormatUrlOmitAll &
      ~(!history_match.match_in_scheme ? 0 : net::kFormatUrlOmitHTTP);
  match.fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          info.url(),
          net::FormatUrl(info.url(), languages_, format_types,
                         net::UnescapeRule::SPACES, NULL, NULL, NULL),
          ChromeAutocompleteSchemeClassifier(profile_));
  std::vector<size_t> offsets =
      OffsetsFromTermMatches(history_match.url_matches());
  base::OffsetAdjuster::Adjustments adjustments;
  match.contents = net::FormatUrlWithAdjustments(
      info.url(), languages_, format_types, net::UnescapeRule::SPACES, NULL,
      NULL, &adjustments);
  base::OffsetAdjuster::AdjustOffsets(adjustments, &offsets);
  history::TermMatches new_matches =
      ReplaceOffsetsInTermMatches(history_match.url_matches(), offsets);
  match.contents_class =
      SpansFromTermMatch(new_matches, match.contents.length(), true);

  // Set |inline_autocompletion| and |allowed_to_be_default_match| if possible.
  if (history_match.can_inline()) {
    DCHECK(!new_matches.empty());
    size_t inline_autocomplete_offset = new_matches[0].offset +
        new_matches[0].length;
    // |inline_autocomplete_offset| may be beyond the end of the
    // |fill_into_edit| if the user has typed an URL with a scheme and the
    // last character typed is a slash.  That slash is removed by the
    // FormatURLWithOffsets call above.
    if (inline_autocomplete_offset < match.fill_into_edit.length()) {
      match.inline_autocompletion =
          match.fill_into_edit.substr(inline_autocomplete_offset);
    }
    match.allowed_to_be_default_match = match.inline_autocompletion.empty() ||
        !PreventInlineAutocomplete(autocomplete_input_);
  }
  match.EnsureUWYTIsAllowedToBeDefault(
      autocomplete_input_.canonicalized_url(),
      TemplateURLServiceFactory::GetForProfile(profile_));

  // Format the description autocomplete presentation.
  match.description = info.title();
  match.description_class = SpansFromTermMatch(
      history_match.title_matches(), match.description.length(), false);

  match.RecordAdditionalInfo("typed count", info.typed_count());
  match.RecordAdditionalInfo("visit count", info.visit_count());
  match.RecordAdditionalInfo("last visit", info.last_visit());

  return match;
}

history::InMemoryURLIndex* HistoryQuickProvider::GetIndex() {
  if (index_for_testing_.get())
    return index_for_testing_.get();

  HistoryService* const history_service =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (!history_service)
    return NULL;

  return history_service->InMemoryIndex();
}
