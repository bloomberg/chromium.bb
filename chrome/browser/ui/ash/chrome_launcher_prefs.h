// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_LAUNCHER_PREFS_H_
#define CHROME_BROWSER_UI_ASH_CHROME_LAUNCHER_PREFS_H_

#include <string>

class PrefServiceSyncable;

namespace base {
class DictionaryValue;
}

namespace ash {

// Path within the dictionary entries in the prefs::kPinnedLauncherApps list
// specifying the extension ID of the app to be pinned by that entry.
extern const char kPinnedAppsPrefAppIDPath[];

// Values used for prefs::kShelfAutoHideBehavior.
extern const char kShelfAutoHideBehaviorAlways[];
extern const char kShelfAutoHideBehaviorNever[];

// Values used for prefs::kShelfAlignment.
extern const char kShelfAlignmentBottom[];
extern const char kShelfAlignmentLeft[];
extern const char kShelfAlignmentRight[];
extern const char kShelfAlignmentTop[];

void RegisterChromeLauncherUserPrefs(PrefServiceSyncable* user_prefs);

base::DictionaryValue* CreateAppDict(const std::string& app_id);

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_CHROME_LAUNCHER_PREFS_H_
