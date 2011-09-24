// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_PROVIDER_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/time.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/history_provider.h"
#include "chrome/browser/autocomplete/shortcuts_provider_shortcut.h"
#include "chrome/browser/history/shortcuts_backend.h"

class Profile;
struct AutocompleteLog;

// Provider of recently autocompleted links. Provides autocomplete suggestions
// from previously selected suggestions. The more often a user selects a
// suggestion for a given search term the higher will be that suggestion's
// ranking for future uses of that search term.
class ShortcutsProvider
    : public AutocompleteProvider,
      public history::ShortcutsBackend::ShortcutsBackendObserver {
 public:
  ShortcutsProvider(ACProviderListener* listener, Profile* profile);
  virtual ~ShortcutsProvider();

  // Performs the autocompletion synchronously. Since no asynch completion is
  // performed |minimal_changes| is ignored.
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes) OVERRIDE;

  virtual void DeleteMatch(const AutocompleteMatch& match) OVERRIDE;

 private:
  friend class ShortcutsProviderTest;
  FRIEND_TEST_ALL_PREFIXES(ShortcutsProviderTest, ClassifyAllMatchesInString);
  FRIEND_TEST_ALL_PREFIXES(ShortcutsProviderTest, CalculateScore);
  FRIEND_TEST_ALL_PREFIXES(ShortcutsProviderTest, DeleteMatch);

  // ShortcutsBackendObserver:
  virtual void OnShortcutsLoaded() OVERRIDE;

  // Clamp relevance scores to ensure none of our matches will become the
  // default. This prevents us from having to worry about inline autocompletion.
  // Made a function instead of a constant to avoid static initialization.
  static int GetMaxScore();

  void DeleteMatchesWithURLs(const std::set<GURL>& urls);
  void DeleteShortcutsWithURLs(const std::set<GURL>& urls);

  // Performs the autocomplete matching and scoring.
  void GetMatches(const AutocompleteInput& input);

  AutocompleteMatch ShortcutToACMatch(
      const AutocompleteInput& input,
      const string16& terms,
      shortcuts_provider::ShortcutMap::const_iterator it);

  // Given |text| and a corresponding base set of classifications
  // |original_class|, adds ACMatchClassification::MATCH markers for all
  // instances of the words from |find_text| within |text| and returns the
  // resulting classifications.
  //
  // For example, given the |text|
  // "Sports and News at sports.somesite.com - visit us!" and |original_class|
  // {{0, NONE}, {18, URL}, {37, NONE}} (marking "sports.somesite.com" as a
  // URL), calling with |find_text| set to "sp ew" would return
  // {{0, MATCH}, {2, NONE}, {12, MATCH}, {14, NONE}, {18, URL|MATCH},
  // {20, URL}, {37, NONE}}
  static ACMatchClassifications ClassifyAllMatchesInString(
      const string16& find_text,
      const string16& text,
      const ACMatchClassifications& original_class);

  // Returns iterator to first item in |shortcuts_map_| matching |keyword|.
  // Returns shortcuts_map_.end() if there are no matches.
  shortcuts_provider::ShortcutMap::const_iterator FindFirstMatch(
      const string16& keyword);

  static int CalculateScore(const string16& terms,
                            const shortcuts_provider::Shortcut& shortcut);
  // For unit-test only.
  void set_shortcuts_backend(history::ShortcutsBackend* shortcuts_backend);

  std::string languages_;
  bool initialized_;

  scoped_refptr<history::ShortcutsBackend> shortcuts_backend_;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_PROVIDER_H_
