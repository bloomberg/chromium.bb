// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_HISTORY_CONTENTS_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_HISTORY_CONTENTS_PROVIDER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/history_provider.h"
#include "chrome/browser/history/history_service.h"

// HistoryContentsProvider is an AutocompleteProvider that provides results from
// the contents (body and/or title) of previously visited pages.
// HistoryContentsProvider gets results from two sources:
// . HistoryService: this provides results for matches in the body/title of
//   previously viewed pages. This is asynchronous.
// . BookmarkModel: provides results for matches in the titles of bookmarks.
//   This is synchronous.
class HistoryContentsProvider : public HistoryProvider {
 public:
  // If |body_only| then only provide results for which there is a match in
  // the body, otherwise also match in the page URL and title.
  HistoryContentsProvider(AutocompleteProviderListener* listener,
                          Profile* profile,
                          bool body_only);

  // As necessary asks the history service for the relevant results. When
  // done SetResults is invoked.
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes) OVERRIDE;
  virtual void Stop(bool clear_cached_results) OVERRIDE;

 private:
  // When processing the results from the history query, this structure points
  // to a single result. It allows the results to be sorted and processed
  // without modifying the larger and slower results structure.
  struct MatchReference {
    MatchReference(const history::URLResult* result,
                   int relevance);

    static bool CompareRelevance(const MatchReference& lhs,
                                 const MatchReference& rhs);

    const history::URLResult* result;
    int relevance;  // Score of relevance computed by CalculateRelevance.
  };

  virtual ~HistoryContentsProvider();

  void QueryComplete(HistoryService::Handle handle,
                     history::QueryResults* results);

  // Converts each MatchingPageResult in results_ to an AutocompleteMatch and
  // adds it to matches_.
  void ConvertResults();

  // Creates and returns an AutocompleteMatch from a MatchingPageResult.
  AutocompleteMatch ResultToMatch(const MatchReference& match_reference);

  // Adds ACMatchClassifications to match from the offset positions in
  // page_result.
  void ClassifyDescription(const history::URLResult& result,
                           AutocompleteMatch* match) const;

  // Calculates and returns the relevance of a particular result. See the
  // chart in autocomplete.h for the list of values this returns.
  int CalculateRelevance(const history::URLResult& result);

  // Return true if the search term can be found in the title of |result|.
  bool MatchInTitle(const history::URLResult& result);

  CancelableRequestConsumerT<int, 0> request_consumer_;

  // The number of times we're returned each different type of result. These are
  // used by CalculateRelevance. Initialized in Start.
  int star_title_count_;
  int star_contents_count_;
  int title_count_;
  int contents_count_;

  // Current autocomplete input type.
  AutocompleteInput::Type input_type_;

  // Whether we should match against the body text only (true) or also against
  // url and titles (false).
  // TODO(mrossetti): Remove body_only_ and MatchInTitle once body_only_
  // becomes permanent.
  bool body_only_;

  // Whether we should trim "http://" from results.
  bool trim_http_;

  // Results from most recent query. These are cached so we don't have to
  // re-issue queries for "minor changes" (which don't affect this provider).
  history::QueryResults results_;

  // Whether results_ is valid (so we can tell invalid apart from empty).
  bool have_results_;

  // Current query string.
  string16 query_;

  DISALLOW_COPY_AND_ASSIGN(HistoryContentsProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_CONTENTS_PROVIDER_H_
