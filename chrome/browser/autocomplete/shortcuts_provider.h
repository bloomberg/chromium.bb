// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_PROVIDER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/time.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/history_provider.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class Profile;
struct AutocompleteLog;

// Provider of recently autocompleted links. Provides autocomplete suggestions
// from previously selected suggestions. The more often a user selects a
// suggestion for a given search term the higher will be that suggestion's
// ranking for future uses of that search term.
class ShortcutsProvider : public AutocompleteProvider,
                          public NotificationObserver {
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

  // The following struct encapsulates one previously selected omnibox shortcut.
  struct Shortcut {
    Shortcut(const string16& text,
             const GURL& url,
             const string16& contents,
             const ACMatchClassifications& contents_class,
             const string16& description,
             const ACMatchClassifications& description_class);
    // Required for STL, we don't use this directly.
    Shortcut();

    string16 text;  // The user's original input string.
    GURL url;       // The corresponding destination URL.

    // Contents and description from the original match, along with their
    // corresponding markup. We need these in order to correctly mark which
    // parts are URLs, dim, etc. However, we strip all MATCH classifications
    // from these since we'll mark the matching portions ourselves as we match
    // the user's current typing against these Shortcuts.
    string16 contents;
    ACMatchClassifications contents_class;
    string16 description;
    ACMatchClassifications description_class;

    base::Time last_access_time;  // Last time shortcut was selected.
    int number_of_hits;           // How many times shortcut was selected.
  };

  // Maps the original match (|text| in the Shortcut) to Shortcut for quick
  // search.
  typedef std::multimap<string16, Shortcut> ShortcutMap;

  // Clamp relevance scores to ensure none of our matches will become the
  // default. This prevents us from having to worry about inline autocompletion.
  static const int kMaxScore;

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  void DeleteMatchesWithURLs(const std::set<GURL>& urls);
  void DeleteShortcutsWithURLs(const std::set<GURL>& urls);

  // Performs the autocomplete matching and scoring.
  void GetMatches(const AutocompleteInput& input);

  AutocompleteMatch ShortcutToACMatch(const AutocompleteInput& input,
                                      const string16& terms,
                                      ShortcutMap::iterator it);

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
  ShortcutMap::iterator FindFirstMatch(const string16& keyword);

  static int CalculateScore(const string16& terms, const Shortcut& shortcut);

  // Helpers dealing with database update.
  // Converts spans vector to comma-separated string.
  static string16 SpansToString(const ACMatchClassifications& value);
  // Coverts comma-separated unsigned integer values into spans vector.
  static ACMatchClassifications SpansFromString(const string16& value);

  std::string languages_;
  NotificationRegistrar notification_registrar_;
  ShortcutMap shortcuts_map_;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_PROVIDER_H_
