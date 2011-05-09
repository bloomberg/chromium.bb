// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_HISTORY_QUICK_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_HISTORY_QUICK_PROVIDER_H_
#pragma once

#include <string>

#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/history_provider.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/in_memory_url_index.h"

class Profile;
class TermMatches;

namespace history {
class HistoryBackend;
}  // namespace history

// This class is an autocomplete provider (a pseudo-internal component of
// the history system) which quickly (and synchronously) provides matching
// results from recently or frequently visited sites in the profile's
// history.
class HistoryQuickProvider : public HistoryProvider {
 public:
  HistoryQuickProvider(ACProviderListener* listener, Profile* profile);

  ~HistoryQuickProvider();

  // AutocompleteProvider. |minimal_changes| is ignored since there
  // is no asynch completion performed.
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes) OVERRIDE;

  virtual void DeleteMatch(const AutocompleteMatch& match) OVERRIDE;

  // Performs the autocomplete matching and scoring.
  void DoAutocomplete();

 private:
  friend class HistoryQuickProviderTest;
  FRIEND_TEST_ALL_PREFIXES(HistoryQuickProviderTest, Spans);
  FRIEND_TEST_ALL_PREFIXES(HistoryQuickProviderTest, Relevance);

  // Creates an AutocompleteMatch from |history_match|. |max_match_score| gives
  // the maximum possible score for the match.
  AutocompleteMatch QuickMatchToACMatch(
      const history::ScoredHistoryMatch& history_match,
      bool prevent_inline_autocomplete,
      int* max_match_score);

  // Determines the relevance score of |history_match|. The maximum allowed
  // score for the match is passed in |max_match_score|. The |max_match_score|
  // is always set to the resulting score minus 1 whenever the match's score
  // has to be limited or is <= to |max_match_score|. This function should be
  // called in a loop with each match in decreasing order of raw score.
  static int CalculateRelevance(
      const history::ScoredHistoryMatch& history_match,
      int* max_match_score);

  // Returns the index that should be used for history lookups.
  history::InMemoryURLIndex* GetIndex();

  // Fill and return an ACMatchClassifications structure given the term
  // matches (|matches|) to highlight where terms were found.
  static ACMatchClassifications SpansFromTermMatch(
      const history::TermMatches& matches,
      size_t text_length,
      bool is_url);

  // Only for use in unittests.  Takes ownership of |index|.
  void SetIndexForTesting(history::InMemoryURLIndex* index);
  AutocompleteInput autocomplete_input_;
  std::string languages_;

  // Only used for testing.
  scoped_ptr<history::InMemoryURLIndex> index_for_testing_;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_QUICK_PROVIDER_H_
