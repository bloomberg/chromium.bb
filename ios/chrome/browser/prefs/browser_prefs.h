// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PREFS_BROWSER_PREFS_H_
#define IOS_CHROME_BROWSER_PREFS_BROWSER_PREFS_H_

class PrefService;

// Migrate/cleanup deprecated prefs in the profile's pref store. Over time, long
// deprecated prefs should be removed as new ones are added, but this call
// should never go away (even if it becomes an empty call for some time) as it
// should remain *the* place to drop deprecated profile prefs at.
void MigrateObsoleteIOSProfilePrefs(PrefService* prefs);

#endif  // IOS_CHROME_BROWSER_PREFS_BROWSER_PREFS_H_
