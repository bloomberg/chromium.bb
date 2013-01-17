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
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_field_trial.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/in_memory_url_index.h"
#include "chrome/browser/history/in_memory_url_index_types.h"
#include "chrome/browser/history/scored_history_match.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/url_parse.h"
#include "googleurl/src/url_util.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

using history::InMemoryURLIndex;
using history::ScoredHistoryMatch;
using history::ScoredHistoryMatches;

bool HistoryQuickProvider::disabled_ = false;

HistoryQuickProvider::HistoryQuickProvider(
    AutocompleteProviderListener* listener,
    Profile* profile)
    : HistoryProvider(listener, profile,
          AutocompleteProvider::TYPE_HISTORY_QUICK),
      languages_(profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)),
      reorder_for_inlining_(false) {
  enum InliningOption {
    INLINING_PROHIBITED = 0,
    INLINING_ALLOWED = 1,
    INLINING_AUTO_BUT_NOT_IN_FIELD_TRIAL = 2,
    INLINING_FIELD_TRIAL_DEFAULT_GROUP = 3,
    INLINING_FIELD_TRIAL_EXPERIMENT_GROUP = 4,
    NUM_OPTIONS = 5
  };
  // should always be overwritten
  InliningOption inlining_option = NUM_OPTIONS;

  const std::string switch_value = CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kOmniboxInlineHistoryQuickProvider);
  if (switch_value == switches::kOmniboxInlineHistoryQuickProviderAllowed) {
    inlining_option = INLINING_ALLOWED;
    always_prevent_inline_autocomplete_ = false;
  } else if (switch_value ==
             switches::kOmniboxInlineHistoryQuickProviderProhibited) {
    inlining_option = INLINING_PROHIBITED;
    always_prevent_inline_autocomplete_ = true;
  } else {
    // We'll assume any other flag means automatic.
    // Automatic means eligible for the field trial.

    // For the field trial stuff to work correctly, we must be running
    // on the same thread as the thread that created the field trial,
    // which happens via a call to AutocompleteFieldTrial::Active in
    // chrome_browser_main.cc on the main thread.  Let's check this to
    // be sure.  We check "if we've heard of the UI thread then we'd better
    // be on it."  The first part is necessary so unit tests pass.  (Many
    // unit tests don't set up the threading naming system; hence
    // CurrentlyOn(UI thread) will fail.)
    DCHECK(!content::BrowserThread::IsWellKnownThread(
               content::BrowserThread::UI) ||
           content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    if (AutocompleteFieldTrial::InDisallowInlineHQPFieldTrial()) {
      if (AutocompleteFieldTrial::
          InDisallowInlineHQPFieldTrialExperimentGroup()) {
        always_prevent_inline_autocomplete_ = true;
        inlining_option = INLINING_FIELD_TRIAL_EXPERIMENT_GROUP;
      } else {
        always_prevent_inline_autocomplete_ = false;
        inlining_option = INLINING_FIELD_TRIAL_DEFAULT_GROUP;
      }
    } else {
      always_prevent_inline_autocomplete_ = false;
      inlining_option = INLINING_AUTO_BUT_NOT_IN_FIELD_TRIAL;
    }
  }

  // Add a beacon to the logs that'll allow us to identify later what
  // inlining state a user is in.  Do this by incrementing a bucket in
  // a histogram, where the bucket represents the user's inlining state.
  UMA_HISTOGRAM_ENUMERATION(
      "Omnibox.InlineHistoryQuickProviderFieldTrialBeacon",
      inlining_option, NUM_OPTIONS);

  reorder_for_inlining_ = CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::
                          kOmniboxHistoryQuickProviderReorderForInlining) ==
      switches::kOmniboxHistoryQuickProviderReorderForInliningEnabled;
}

void HistoryQuickProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  matches_.clear();
  if (disabled_)
    return;

  // Don't bother with INVALID and FORCED_QUERY.  Also pass when looking for
  // BEST_MATCH and there is no inline autocompletion because none of the HQP
  // matches can score highly enough to qualify.
  if ((input.type() == AutocompleteInput::INVALID) ||
      (input.type() == AutocompleteInput::FORCED_QUERY) ||
      (input.matches_requested() == AutocompleteInput::BEST_MATCH &&
       input.prevent_inline_autocomplete()))
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
      base::Histogram* counter = base::Histogram::FactoryGet(
          name, 1, 1000, 50, base::Histogram::kUmaTargetedHistogramFlag);
      counter->Add(static_cast<int>((end_time - start_time).InMilliseconds()));
    }
    UpdateStarredStateOfMatches();
  }
}

void HistoryQuickProvider::DeleteMatch(const AutocompleteMatch& match) {
  DCHECK(match.deletable);
  DCHECK(match.destination_url.is_valid());
  // Delete the match from the InMemoryURLIndex.
  GetIndex()->DeleteURL(match.destination_url);
  DeleteMatchFromMatches(match);
}

HistoryQuickProvider::~HistoryQuickProvider() {}

void HistoryQuickProvider::DoAutocomplete() {
  // Get the matching URLs from the DB.
  string16 term_string = autocomplete_input_.text();
  ScoredHistoryMatches matches = GetIndex()->HistoryItemsForTerms(term_string);
  if (matches.empty())
    return;

  // If we're allowed to reorder results in order to get an inlineable
  // result to appear first (and hence have a HistoryQuickProvider
  // suggestion possibly appear first), find the first inlineable
  // result and then swap it to the front.  Obviously, don't do this
  // if we're told to prevent inline autocompletion.  (If we're told
  // we're going to prevent inline autocompletion, we're going to
  // later demote the score of all results so none will be inlined.
  // Hence there's no need to reorder the results so an inlineable one
  // appears first.)
  if (reorder_for_inlining_ &&
      !PreventInlineAutocomplete(autocomplete_input_)) {
    for (ScoredHistoryMatches::iterator i(matches.begin());
         (i != matches.end()) &&
             (i->raw_score >= AutocompleteResult::kLowestDefaultScore);
         ++i) {
      if (i->can_inline) {  // this test is only true once because of the break
        if (i != matches.begin())
          std::rotate(matches.begin(), i, i + 1);
        break;
      }
    }
  }

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
      autocomplete_input_.canonicalized_url().is_valid() &&
      (autocomplete_input_.type() != AutocompleteInput::QUERY) &&
      (autocomplete_input_.type() != AutocompleteInput::FORCED_QUERY) &&
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
        const std::string host(UTF16ToUTF8(autocomplete_input_.text().substr(
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
          if (net::RegistryControlledDomainService::GetRegistryLength(
              host, false) == 0) {
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
  // see.  Also, if we're not allowing inline autocompletions in
  // general or the current best suggestion isn't inlineable,
  // artificially reduce the starting |max_match_score| (which
  // therefore applies to all results) to something low enough that
  // guarantees no result will be offered as an autocomplete
  // suggestion.  Also do a similar reduction if we think there will be
  // a URL-what-you-typed match.  (We want URL-what-you-typed matches for
  // visited URLs to beat out any longer URLs, no matter how frequently
  // they're visited.)  The strength of this last reduction depends on the
  // likely score for the URL-what-you-typed result.
  int max_match_score = (PreventInlineAutocomplete(autocomplete_input_) ||
      !matches.begin()->can_inline) ?
      (AutocompleteResult::kLowestDefaultScore - 1) :
      matches.begin()->raw_score;
  if (will_have_url_what_you_typed_match_first) {
    max_match_score = std::min(max_match_score,
        url_what_you_typed_match_score - 1);
  }
  for (ScoredHistoryMatches::const_iterator match_iter = matches.begin();
       match_iter != matches.end(); ++match_iter) {
    const ScoredHistoryMatch& history_match(*match_iter);
    // Set max_match_score to the score we'll assign this result:
    max_match_score = std::min(max_match_score, history_match.raw_score);
    matches_.push_back(QuickMatchToACMatch(history_match, max_match_score));
    // Mark this max_match_score as being used:
    max_match_score--;
  }
}

AutocompleteMatch HistoryQuickProvider::QuickMatchToACMatch(
    const ScoredHistoryMatch& history_match,
    int score) {
  const history::URLRow& info = history_match.url_info;
  AutocompleteMatch match(this, score, !!info.visit_count(),
      history_match.url_matches.empty() ?
          AutocompleteMatch::HISTORY_TITLE : AutocompleteMatch::HISTORY_URL);
  match.typed_count = info.typed_count();
  match.destination_url = info.url();
  DCHECK(match.destination_url.is_valid());

  // Format the URL autocomplete presentation.
  std::vector<size_t> offsets =
      OffsetsFromTermMatches(history_match.url_matches);
  const net::FormatUrlTypes format_types = net::kFormatUrlOmitAll &
      ~(!history_match.match_in_scheme ? 0 : net::kFormatUrlOmitHTTP);
  match.fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(info.url(),
          net::FormatUrlWithOffsets(info.url(), languages_, format_types,
              net::UnescapeRule::SPACES, NULL, NULL, &offsets));
  history::TermMatches new_matches =
      ReplaceOffsetsInTermMatches(history_match.url_matches, offsets);
  match.contents = net::FormatUrl(info.url(), languages_, format_types,
              net::UnescapeRule::SPACES, NULL, NULL, NULL);
  match.contents_class =
      SpansFromTermMatch(new_matches, match.contents.length(), true);

  if (!history_match.can_inline) {
    match.inline_autocomplete_offset = string16::npos;
  } else {
    DCHECK(!new_matches.empty());
    match.inline_autocomplete_offset = new_matches[0].offset +
        new_matches[0].length;
    // The following will happen if the user has typed an URL with a scheme
    // and the last character typed is a slash because that slash is removed
    // by the FormatURLWithOffsets call above.
    if (match.inline_autocomplete_offset > match.fill_into_edit.length())
      match.inline_autocomplete_offset = match.fill_into_edit.length();
  }

  // Format the description autocomplete presentation.
  match.description = info.title();
  match.description_class = SpansFromTermMatch(
      history_match.title_matches, match.description.length(), false);

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

// static
ACMatchClassifications HistoryQuickProvider::SpansFromTermMatch(
    const history::TermMatches& matches,
    size_t text_length,
    bool is_url) {
  ACMatchClassification::Style url_style =
      is_url ? ACMatchClassification::URL : ACMatchClassification::NONE;
  ACMatchClassifications spans;
  if (matches.empty()) {
    if (text_length)
      spans.push_back(ACMatchClassification(0, url_style));
    return spans;
  }
  if (matches[0].offset)
    spans.push_back(ACMatchClassification(0, url_style));
  size_t match_count = matches.size();
  for (size_t i = 0; i < match_count;) {
    size_t offset = matches[i].offset;
    spans.push_back(ACMatchClassification(offset,
        ACMatchClassification::MATCH | url_style));
    // Skip all adjacent matches.
    do {
      offset += matches[i].length;
      ++i;
    } while ((i < match_count) && (offset == matches[i].offset));
    if (offset < text_length)
      spans.push_back(ACMatchClassification(offset, url_style));
  }

  return spans;
}
