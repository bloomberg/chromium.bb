// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_PROMPT_PREFS_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_PROMPT_PREFS_H_

#include "base/basictypes.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Helper class for getting, changing bookmark prompt related preferences.
class BookmarkPromptPrefs {
 public:
  // Constructs and associates to |prefs|. Further operations occurred on
  // associated |prefs|.
  explicit BookmarkPromptPrefs(PrefService* prefs);
  ~BookmarkPromptPrefs();

  // Disables bookmark prompt feature.
  void DisableBookmarkPrompt();

  // Returns number of times bookmark prompt displayed so far.
  int GetPromptImpressionCount() const;

  // Increments bookmark prompt impression counter.
  void IncrementPromptImpressionCount();

  // Returns true if bookmark prompt feature enabled, otherwise false.
  bool IsBookmarkPromptEnabled() const;

  // Registers user preferences used by bookmark prompt feature.
  static void RegisterUserPrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  PrefService* prefs_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(BookmarkPromptPrefs);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_PROMPT_PREFS_H_
