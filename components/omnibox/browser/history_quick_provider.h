// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_HISTORY_QUICK_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_HISTORY_QUICK_PROVIDER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/history/core/browser/history_types.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/history_provider.h"
#include "components/omnibox/browser/in_memory_url_index.h"

struct ScoredHistoryMatch;

// This class is an autocomplete provider (a pseudo-internal component of
// the history system) which quickly (and synchronously) provides matching
// results from recently or frequently visited sites in the profile's
// history.
class HistoryQuickProvider : public HistoryProvider {
 public:
  explicit HistoryQuickProvider(AutocompleteProviderClient* client);

  // AutocompleteProvider. |minimal_changes| is ignored since there is no asynch
  // completion performed.
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const override;

  // Disable this provider. For unit testing purposes only. This is required
  // because this provider is closely associated with the HistoryURLProvider
  // and in order to properly test the latter the HistoryQuickProvider must
  // be disabled.
  // TODO(mrossetti): Eliminate this once the HUP has been refactored.
  static void set_disabled(bool disabled) { disabled_ = disabled; }

 private:
// Flaky leaks on ASAN LSAN (crbug.com/1010691).
#if defined(ADDRESS_SANITIZER)
#define MAYBE_HistoryQuickProviderTest DISABLED_HistoryQuickProviderTest
#else
#define MAYBE_HistoryQuickProviderTest HistoryQuickProviderTest
#endif

  friend class MAYBE_HistoryQuickProviderTest;
  FRIEND_TEST_ALL_PREFIXES(MAYBE_HistoryQuickProviderTest, Spans);
  FRIEND_TEST_ALL_PREFIXES(MAYBE_HistoryQuickProviderTest, Relevance);
  FRIEND_TEST_ALL_PREFIXES(MAYBE_HistoryQuickProviderTest, DoTrimHttpScheme);
  FRIEND_TEST_ALL_PREFIXES(MAYBE_HistoryQuickProviderTest,
                           DontTrimHttpSchemeIfInputHasScheme);
  FRIEND_TEST_ALL_PREFIXES(MAYBE_HistoryQuickProviderTest,
                           DontTrimHttpSchemeIfInputMatches);
  FRIEND_TEST_ALL_PREFIXES(MAYBE_HistoryQuickProviderTest,
                           DontTrimHttpsSchemeIfInputHasScheme);
  FRIEND_TEST_ALL_PREFIXES(MAYBE_HistoryQuickProviderTest, DoTrimHttpsScheme);
  FRIEND_TEST_ALL_PREFIXES(MAYBE_HistoryQuickProviderTest,
                           CorrectAutocompleteWithTrailingSlash);

  ~HistoryQuickProvider() override;

  // Performs the autocomplete matching and scoring.
  void DoAutocomplete();

  // Calculates the initial max match score for applying to matches, lowering
  // it if we believe that there will be a URL-what-you-typed match.
  int FindMaxMatchScore(const ScoredHistoryMatches& matches);

  // Creates an AutocompleteMatch from |history_match|, assigning it
  // the score |score|.
  AutocompleteMatch QuickMatchToACMatch(const ScoredHistoryMatch& history_match,
                                        int score);

  AutocompleteInput autocomplete_input_;
  InMemoryURLIndex* in_memory_url_index_;  // Not owned by this class.

  // This provider is disabled when true.
  static bool disabled_;

  DISALLOW_COPY_AND_ASSIGN(HistoryQuickProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_HISTORY_QUICK_PROVIDER_H_
