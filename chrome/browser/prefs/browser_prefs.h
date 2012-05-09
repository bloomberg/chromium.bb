// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_BROWSER_PREFS_H__
#define CHROME_BROWSER_PREFS_BROWSER_PREFS_H__
#pragma once

class PrefService;
class Profile;

namespace browser {

// Bitmask for kMultipleProfilePrefMigration.
enum MigratedPreferences {
  NO_PREFS = 0,
  DNS_PREFS = 1 << 0,
  WINDOWS_PREFS = 1 << 1,
  GOOGLE_URL_TRACKER_PREFS = 1 << 2,
};

// Makes the PrefService objects aware of all the prefs.
void RegisterLocalState(PrefService* local_state);
void RegisterUserPrefs(PrefService* user_prefs);
// Migrate prefs from local_state to |profile|'s pref store.
void MigrateBrowserPrefs(Profile* profile, PrefService* local_state);

} // namespace browser

#endif  // CHROME_BROWSER_PREFS_BROWSER_PREFS_H__
