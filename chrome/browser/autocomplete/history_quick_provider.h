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

  AutocompleteMatch QuickMatchToACMatch(
      const history::ScoredHistoryMatch& history_match,
      size_t match_number,
      bool prevent_inline_autocomplete,
      int* next_dont_inline_score);

  // Determines the relevance for some input, given its type and which match it
  // is.  If |match_type| is NORMAL, |match_number| is a number
  // [0, kMaxSuggestions) indicating the relevance of the match (higher == more
  // relevant).  For other values of |match_type|, |match_number| is ignored.
  static int CalculateRelevance(int raw_score,
                                AutocompleteInput::Type input_type,
                                MatchType match_type,
                                size_t match_number);

  // Returns the index that should be used for history lookups.
  history::InMemoryURLIndex* GetIndex();

  // Fill and return an ACMatchClassifications structure given the term
  // matches (|matches|) to highlight where terms were found.
  static ACMatchClassifications SpansFromTermMatch(
      const history::TermMatches& matches,
      size_t text_length);

  // Only for use in unittests.  Takes ownership of |index|.
  void SetIndexForTesting(history::InMemoryURLIndex* index);
  AutocompleteInput autocomplete_input_;
  std::string languages_;

  // Only used for testing.
  scoped_ptr<history::InMemoryURLIndex> index_for_testing_;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_QUICK_PROVIDER_H_
