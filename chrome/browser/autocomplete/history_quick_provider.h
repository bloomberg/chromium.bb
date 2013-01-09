// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_HISTORY_QUICK_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_HISTORY_QUICK_PROVIDER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
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
  HistoryQuickProvider(AutocompleteProviderListener* listener,
                       Profile* profile);

  // AutocompleteProvider. |minimal_changes| is ignored since there is no asynch
  // completion performed.
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

  // True if we're allowed to reorder results depending on
  // inlineability in order to assign higher relevance scores.
  // Consider a case where ScoredHistoryMatch provides results x and
  // y, where x is not inlineable and has a score of 3000 and y is
  // inlineable and has a score of 2500.  If reorder_for_inlining_ is
  // false, then x gets demoted to a non-inlineable score (1199) and y
  // gets demoted to a lower score (1198) because we try to preserve
  // the order.  On the other hand, if reorder_for_inlining_ is true,
  // then y keeps its score of 2500 and x gets demoted to 2499 in
  // order to follow y.  There will not be any problems with an
  // unexpected inline because the non-inlineable result x scores
  // lower than the inlineable one.
  // TODO(mpearson): remove this variable after we're done experimenting.
  // (This member is meant to only exist for experimentation purposes.
  // Once we know which behavior is better, we should rip out this variable
  // and make the best behavior the default.)
  bool reorder_for_inlining_;

  // Only used for testing.
  scoped_ptr<history::InMemoryURLIndex> index_for_testing_;

  // This provider is disabled when true.
  static bool disabled_;

  DISALLOW_COPY_AND_ASSIGN(HistoryQuickProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_QUICK_PROVIDER_H_
