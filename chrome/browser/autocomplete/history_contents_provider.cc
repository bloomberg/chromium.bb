// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/history_contents_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/url_util.h"
#include "net/base/net_util.h"

using base::TimeTicks;
using history::HistoryDatabase;

namespace {

// Number of days to search for full text results. The longer this is, the more
// time it will take.
const int kDaysToSearch = 30;

}  // namespace

HistoryContentsProvider::MatchReference::MatchReference(
    const history::URLResult* result,
    int relevance)
    : result(result),
      relevance(relevance) {
}

// static
bool HistoryContentsProvider::MatchReference::CompareRelevance(
    const HistoryContentsProvider::MatchReference& lhs,
    const HistoryContentsProvider::MatchReference& rhs) {
  if (lhs.relevance != rhs.relevance)
    return lhs.relevance > rhs.relevance;
  return lhs.result->last_visit() > rhs.result->last_visit();
}

HistoryContentsProvider::HistoryContentsProvider(
    AutocompleteProviderListener* listener,
    Profile* profile,
    bool body_only)
    : HistoryProvider(listener, profile,
          AutocompleteProvider::TYPE_HISTORY_CONTENTS),
      star_title_count_(0),
      star_contents_count_(0),
      title_count_(0),
      contents_count_(0),
      input_type_(AutocompleteInput::INVALID),
      body_only_(body_only),
      trim_http_(false),
      have_results_(false) {
}

void HistoryContentsProvider::Start(const AutocompleteInput& input,
                                    bool minimal_changes) {
  matches_.clear();

  if (input.text().empty() || (input.type() == AutocompleteInput::INVALID) ||
      (input.type() == AutocompleteInput::FORCED_QUERY) ||
      !profile_ ||
      // The history service or bookmark bar model must exist.
      !(HistoryServiceFactory::GetForProfile(profile_,
                                             Profile::EXPLICIT_ACCESS) ||
        BookmarkModelFactory::GetForProfile(profile_))) {
    Stop(false);
    return;
  }

  // TODO(pkasting): http://b/888148 We disallow URL input and "URL-like" input
  // (REQUESTED_URL or UNKNOWN with dots) because we get poor results for it,
  // but we could get better results if we did better tokenizing instead.
  if ((input.type() == AutocompleteInput::URL) ||
      (((input.type() == AutocompleteInput::REQUESTED_URL) ||
        (input.type() == AutocompleteInput::UNKNOWN)) &&
       (input.text().find('.') != string16::npos))) {
    Stop(false);
    return;
  }

  if (input.matches_requested() == AutocompleteInput::BEST_MATCH) {
    // None of our results are applicable for best match.
    Stop(false);
    return;
  }

  // Change input type so matches will be marked up properly.
  input_type_ = input.type();
  trim_http_ = !HasHTTPScheme(input.text());

  // Decide what to do about any previous query/results.
  if (!minimal_changes) {
    // Any in-progress request is irrelevant, cancel it.
    Stop(false);
  } else if (have_results_) {
    // We finished the previous query and still have its results.  Mark them up
    // again for the new input.
    ConvertResults();
    return;
  } else if (!done_) {
    // We're still running the previous query on the HistoryService.  If we're
    // allowed to keep running it, do so, and when it finishes, its results will
    // get marked up for this new input.  In synchronous_only mode, cancel the
    // history query.
    if (input.matches_requested() != AutocompleteInput::ALL_MATCHES) {
      done_ = true;
      request_consumer_.CancelAllRequests();
    }
    ConvertResults();
    return;
  }

  if (!results_.empty()) {
    // Clear the results. We swap in an empty one as the easy way to clear it.
    history::QueryResults empty_results;
    results_.Swap(&empty_results);
  }

  if (input.matches_requested() == AutocompleteInput::ALL_MATCHES) {
    HistoryService* history =
        HistoryServiceFactory::GetForProfile(profile_,
                                             Profile::EXPLICIT_ACCESS);
    if (history) {
      done_ = false;

      history::QueryOptions options;
      options.body_only = body_only_;
      options.SetRecentDayRange(kDaysToSearch);
      options.max_count = kMaxMatches;
      history->QueryHistory(input.text(), options,
          &request_consumer_,
          base::Bind(&HistoryContentsProvider::QueryComplete,
                     base::Unretained(this)));
    }
  }
}

void HistoryContentsProvider::Stop(bool clear_cached_results) {
  done_ = true;
  request_consumer_.CancelAllRequests();

  // Clear the results. We swap in an empty one as the easy way to clear it.
  history::QueryResults empty_results;
  results_.Swap(&empty_results);
  have_results_ = false;
}

HistoryContentsProvider::~HistoryContentsProvider() {
}

void HistoryContentsProvider::QueryComplete(HistoryService::Handle handle,
                                            history::QueryResults* results) {
  DCHECK(results_.empty());
  results_.Swap(results);
  have_results_ = true;
  ConvertResults();

  done_ = true;
  if (listener_)
    listener_->OnProviderUpdate(!matches_.empty());
}

void HistoryContentsProvider::ConvertResults() {
  // Reset the relevance counters so that result relevance won't vary on
  // subsequent passes of ConvertResults.
  star_title_count_ = star_contents_count_ = title_count_ = contents_count_ = 0;

  // Make the result references and score the results.
  std::vector<MatchReference> result_refs;
  result_refs.reserve(results_.size());

  // |template_url_service| or |template_url| can be NULL in unit tests.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  TemplateURL* template_url = template_url_service ?
      template_url_service->GetDefaultSearchProvider() : NULL;

  // Results are sorted in decreasing order so we run the loop backwards so that
  // the relevance increment favors the higher ranked results.
  for (std::vector<history::URLResult*>::const_reverse_iterator i =
       results_.rbegin(); i != results_.rend(); ++i) {
    history::URLResult* result = *i;
    // Culls results corresponding to queries from the default search engine.
    // These are low-quality, difficult-to-understand matches for users, and the
    // SearchProvider should surface past queries in a better way anyway.
    if (!template_url || !template_url->IsSearchURL(result->url())) {
      MatchReference ref(result, CalculateRelevance(*result));
      result_refs.push_back(ref);
    }
  }

  // Get the top matches and add them.
  size_t max_for_provider = std::min(kMaxMatches, result_refs.size());
  std::partial_sort(result_refs.begin(), result_refs.begin() + max_for_provider,
                    result_refs.end(), &MatchReference::CompareRelevance);
  matches_.clear();
  for (size_t i = 0; i < max_for_provider; i++) {
    AutocompleteMatch match(ResultToMatch(result_refs[i]));
    matches_.push_back(match);
  }
}

// TODO(mrossetti): Remove MatchInTitle once body_only_ becomes permanent.
bool HistoryContentsProvider::MatchInTitle(const history::URLResult& result) {
  return !body_only_ && !result.title_match_positions().empty();
}

AutocompleteMatch HistoryContentsProvider::ResultToMatch(
    const MatchReference& match_reference) {
  const history::URLResult& result = *match_reference.result;
  AutocompleteMatch match(this, match_reference.relevance, true,
      MatchInTitle(result) ?
          AutocompleteMatch::HISTORY_TITLE : AutocompleteMatch::HISTORY_BODY);
  match.contents = StringForURLDisplay(result.url(), true, trim_http_);
  match.fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(result.url(),
                                                              match.contents);
  match.destination_url = result.url();
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.description = result.title();
  BookmarkModel* bm_model = BookmarkModelFactory::GetForProfile(profile_);
  match.starred = (bm_model && bm_model->IsBookmarked(result.url()));

  ClassifyDescription(result, &match);
  return match;
}

void HistoryContentsProvider::ClassifyDescription(
    const history::URLResult& result,
    AutocompleteMatch* match) const {
  const Snippet::MatchPositions& title_matches = result.title_match_positions();

  size_t offset = 0;
  if (!title_matches.empty()) {
    // Classify matches in the title.
    for (Snippet::MatchPositions::const_iterator i = title_matches.begin();
         i != title_matches.end(); ++i) {
      if (i->first != offset) {
        match->description_class.push_back(
            ACMatchClassification(offset, ACMatchClassification::NONE));
      }
      match->description_class.push_back(
            ACMatchClassification(i->first, ACMatchClassification::MATCH));
      offset = i->second;
    }
  }
  if (offset != result.title().length()) {
    match->description_class.push_back(
        ACMatchClassification(offset, ACMatchClassification::NONE));
  }
}

int HistoryContentsProvider::CalculateRelevance(
    const history::URLResult& result) {
  const bool in_title = MatchInTitle(result);
  BookmarkModel* bm_model = BookmarkModelFactory::GetForProfile(profile_);
  if (!bm_model || !bm_model->IsBookmarked(result.url()))
    return in_title ? (700 + title_count_++) : (500 + contents_count_++);
  return in_title ?
      (1000 + star_title_count_++) : (550 + star_contents_count_++);
}
