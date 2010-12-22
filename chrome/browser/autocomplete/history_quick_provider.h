// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_HISTORY_QUICK_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_HISTORY_QUICK_PROVIDER_H_
#pragma once

#include <string>

#include "chrome/browser/autocomplete/history_provider.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/in_memory_url_index.h"

class Profile;

namespace history {
class HistoryBackend;
}  // namespace history

// This class is an autocomplete provider (a pseudo-internal component of
// the history system) which quickly (and synchronously) provides matching
// results from recently or frequently visited sites in the profile's
// history.
//
// TODO(mrossetti): Review to see if the following applies since we're not
// using the database during the autocomplete pass.
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

  AutocompleteMatch QuickMatchToACMatch(
      const history::ScoredHistoryMatch& history_match,
      MatchType match_type,
      size_t match_number);

  // Breaks a string down into individual words and return as a vector with
  // the individual words in their original order.
  static history::InMemoryURLIndex::String16Vector WordVectorFromString16(
      const string16& uni_string);

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

  // Only for use in unittests.  Takes ownership of |index|.
  void SetIndexForTesting(history::InMemoryURLIndex* index);
  AutocompleteInput autocomplete_input_;
  bool trim_http_;
  std::string languages_;

  // Only used for testing.
  scoped_ptr<history::InMemoryURLIndex> index_for_testing_;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_QUICK_PROVIDER_H_
