// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
struct ScoredHistoryMatch;
}  // namespace history

// This class is an autocomplete provider (a pseudo-internal component of
// the history system) which quickly (and synchronously) provides matching
// results from recently or frequently visited sites in the profile's
// history.
class HistoryQuickProvider : public HistoryProvider {
 public:
  HistoryQuickProvider(ACProviderListener* listener, Profile* profile);

  // AutocompleteProvider. |minimal_changes| is ignored since there
  // is no asynch completion performed.
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes) OVERRIDE;

  virtual void DeleteMatch(const AutocompleteMatch& match) OVERRIDE;

  // Disable this provider. For unit testing purposes only. This is required
  // because this provider is closely associated with the HistoryURLProvider
  // and in order to properly test the latter the HistoryQuickProvider must
  // be disabled.
  // TODO(mrossetti): Eliminate this once the HUP has been refactored.
  static void set_disabled(bool disabled) { disabled_ = disabled; }

 private:
  friend class HistoryQuickProviderTest;
  FRIEND_TEST_ALL_PREFIXES(HistoryQuickProviderTest, Spans);
  FRIEND_TEST_ALL_PREFIXES(HistoryQuickProviderTest, Relevance);

  virtual ~HistoryQuickProvider();

  // Performs the autocomplete matching and scoring.
  void DoAutocomplete();

  // Creates an AutocompleteMatch from |history_match|, assigning it
  // the score |score|.
  AutocompleteMatch QuickMatchToACMatch(
      const history::ScoredHistoryMatch& history_match,
      int score);

  // Returns the index that should be used for history lookups.
  history::InMemoryURLIndex* GetIndex();

  // Fill and return an ACMatchClassifications structure given the term
  // matches (|matches|) to highlight where terms were found.
  static ACMatchClassifications SpansFromTermMatch(
      const history::TermMatches& matches,
      size_t text_length,
      bool is_url);

  // Only for use in unittests.  Takes ownership of |index|.
  void set_index(history::InMemoryURLIndex* index) {
    index_for_testing_.reset(index);
  }

  AutocompleteInput autocomplete_input_;
  std::string languages_;

  // Only used for testing.
  scoped_ptr<history::InMemoryURLIndex> index_for_testing_;

  // This provider is disabled when true.
  static bool disabled_;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_QUICK_PROVIDER_H_
