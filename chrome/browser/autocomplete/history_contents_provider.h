// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_HISTORY_CONTENTS_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_HISTORY_CONTENTS_PROVIDER_H_
#pragma once

#include "chrome/browser/autocomplete/history_provider.h"
#include "chrome/browser/history/history.h"

namespace bookmark_utils {
struct TitleMatch;
}

// HistoryContentsProvider is an AutocompleteProvider that provides results from
// the contents (body and/or title) of previously visited pages.
// HistoryContentsProvider gets results from two sources:
// . HistoryService: this provides results for matches in the body/title of
//   previously viewed pages. This is asynchronous.
// . BookmarkModel: provides results for matches in the titles of bookmarks.
//   This is synchronous.
class HistoryContentsProvider : public HistoryProvider {
 public:
  HistoryContentsProvider(ACProviderListener* listener, Profile* profile);

  // As necessary asks the history service for the relevant results. When
  // done SetResults is invoked.
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  ~HistoryContentsProvider();

  void QueryComplete(HistoryService::Handle handle,
                     history::QueryResults* results);

  // Converts each MatchingPageResult in results_ to an AutocompleteMatch and
  // adds it to matches_.
  void ConvertResults();

  // Creates and returns an AutocompleteMatch from a MatchingPageResult.
  AutocompleteMatch ResultToMatch(const history::URLResult& result,
                                  int score);

  // Adds ACMatchClassifications to match from the offset positions in
  // page_result.
  void ClassifyDescription(const history::URLResult& result,
                           AutocompleteMatch* match) const;

  // Calculates and returns the relevance of a particular result. See the
  // chart in autocomplete.h for the list of values this returns.
  int CalculateRelevance(const history::URLResult& result);

  // Queries the bookmarks for any bookmarks whose title matches input. All
  // matches are added directly to results_.
  void QueryBookmarks(const AutocompleteInput& input);

  // Converts a BookmarkModel::TitleMatch to a QueryResult and adds it to
  // results_.
  void AddBookmarkTitleMatchToResults(const bookmark_utils::TitleMatch& match);

  CancelableRequestConsumerT<int, 0> request_consumer_;

  // The number of times we're returned each different type of result. These are
  // used by CalculateRelevance. Initialized in Start.
  int star_title_count_;
  int star_contents_count_;
  int title_count_;
  int contents_count_;

  // Current autocomplete input type.
  AutocompleteInput::Type input_type_;

  // Whether we should trim "http://" from results.
  bool trim_http_;

  // Results from most recent query. These are cached so we don't have to
  // re-issue queries for "minor changes" (which don't affect this provider).
  history::QueryResults results_;

  // Whether results_ is valid (so we can tell invalid apart from empty).
  bool have_results_;

  // Current query string.
  std::wstring query_;

  DISALLOW_COPY_AND_ASSIGN(HistoryContentsProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_CONTENTS_PROVIDER_H_
