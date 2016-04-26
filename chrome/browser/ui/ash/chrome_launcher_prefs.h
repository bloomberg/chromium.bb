// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_LAUNCHER_PREFS_H_
#define CHROME_BROWSER_UI_ASH_CHROME_LAUNCHER_PREFS_H_

#include <string>

#include "ash/shelf/shelf_types.h"

class PrefService;

namespace base {
class DictionaryValue;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace ash {

// Path within the dictionary entries in the prefs::kPinnedLauncherApps list
// specifying the extension ID of the app to be pinned by that entry.
extern const char kPinnedAppsPrefAppIDPath[];

extern const char kPinnedAppsPrefPinnedByPolicy[];

// Values used for prefs::kShelfAutoHideBehavior.
extern const char kShelfAutoHideBehaviorAlways[];
extern const char kShelfAutoHideBehaviorNever[];

// Values used for prefs::kShelfAlignment.
extern const char kShelfAlignmentBottom[];
extern const char kShelfAlignmentLeft[];
extern const char kShelfAlignmentRight[];

void RegisterChromeLauncherUserPrefs(
    user_prefs::PrefRegistrySyncable* registry);

base::DictionaryValue* CreateAppDict(const std::string& app_id);

// Get or set the shelf auto hide behavior preference for a root window.
ShelfAutoHideBehavior GetShelfAutoHideBehaviorPref(PrefService* prefs,
                                                   int64_t display_id);
void SetShelfAutoHideBehaviorPref(PrefService* prefs,
                                  int64_t display_id,
                                  ShelfAutoHideBehavior behavior);

// Get or set the shelf alignment preference for a root window.
wm::ShelfAlignment GetShelfAlignmentPref(PrefService* prefs,
                                         int64_t display_id);
void SetShelfAlignmentPref(PrefService* prefs,
                           int64_t display_id,
                           wm::ShelfAlignment alignment);

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_CHROME_LAUNCHER_PREFS_H_
