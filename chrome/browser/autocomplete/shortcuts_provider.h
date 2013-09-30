// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_PROVIDER_H_

#include <map>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/history/shortcuts_backend.h"

class Profile;

// Provider of recently autocompleted links. Provides autocomplete suggestions
// from previously selected suggestions. The more often a user selects a
// suggestion for a given search term the higher will be that suggestion's
// ranking for future uses of that search term.
class ShortcutsProvider
    : public AutocompleteProvider,
      public history::ShortcutsBackend::ShortcutsBackendObserver {
 public:
  ShortcutsProvider(AutocompleteProviderListener* listener, Profile* profile);

  // Performs the autocompletion synchronously. Since no asynch completion is
  // performed |minimal_changes| is ignored.
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes) OVERRIDE;

  virtual void DeleteMatch(const AutocompleteMatch& match) OVERRIDE;

 private:
  friend class ClassifyTest;
  friend class ShortcutsProviderTest;
  FRIEND_TEST_ALL_PREFIXES(ShortcutsProviderTest, CalculateScore);
  FRIEND_TEST_ALL_PREFIXES(ShortcutsProviderTest, DeleteMatch);

  typedef std::multimap<char16, string16> WordMap;

  virtual ~ShortcutsProvider();

  // ShortcutsBackendObserver:
  virtual void OnShortcutsLoaded() OVERRIDE;

  // Performs the autocomplete matching and scoring.
  void GetMatches(const AutocompleteInput& input);

  AutocompleteMatch ShortcutToACMatch(
      int relevance,
      const string16& terms,
      const history::ShortcutsBackend::Shortcut& shortcut);

  // Returns a map mapping characters to groups of words from |text| that start
  // with those characters, ordered lexicographically descending so that longer
  // words appear before their prefixes (if any) within a particular
  // equal_range().
  static WordMap CreateWordMapForString(const string16& text);

  // Given |text| and a corresponding base set of classifications
  // |original_class|, adds ACMatchClassification::MATCH markers for all
  // instances of the words from |find_words| within |text| and returns the
  // resulting classifications.  (|find_text| is provided as the original string
  // used to create |find_words|.  This is supplied because it's common for this
  // to be a prefix of |text|, so we can quickly check for that and mark that
  // entire substring as a match before proceeding with the more generic
  // algorithm.)
  //
  // For example, given the |text|
  // "Sports and News at sports.somesite.com - visit us!" and |original_class|
  // {{0, NONE}, {18, URL}, {37, NONE}} (marking "sports.somesite.com" as a
  // URL), calling with |find_text| set to "sp ew" would return
  // {{0, MATCH}, {2, NONE}, {12, MATCH}, {14, NONE}, {18, URL|MATCH},
  // {20, URL}, {37, NONE}}.
  //
  // |find_words| should be as constructed by CreateWordMapForString(find_text).
  //
  // |find_text| (and thus |find_words|) are expected to be lowercase.  |text|
  // will be lowercased in this function.
  static ACMatchClassifications ClassifyAllMatchesInString(
      const string16& find_text,
      const WordMap& find_words,
      const string16& text,
      const ACMatchClassifications& original_class);

  // Returns iterator to first item in |shortcuts_map_| matching |keyword|.
  // Returns shortcuts_map_.end() if there are no matches.
  history::ShortcutsBackend::ShortcutMap::const_iterator FindFirstMatch(
      const string16& keyword,
      history::ShortcutsBackend* backend);

  int CalculateScore(
      const string16& terms,
      const history::ShortcutsBackend::Shortcut& shortcut,
      int max_relevance);

  std::string languages_;
  bool initialized_;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_PROVIDER_H_
