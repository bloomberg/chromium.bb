// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_NAVIGATION_UTIL_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_NAVIGATION_UTIL_WIN_H_

#include "chrome/browser/ui/browser.h"

namespace chrome_cleaner_util {

// Returns the last active tabbed browser or NULL if no such browser currently
// exists.
Browser* FindBrowser();

// Returns true if the settings page is the currently active tab.
bool SettingsPageIsActiveTab(Browser* browser);

// Opens a new settings tab in |browser| with the given |disposition|.
void OpenSettingsPage(Browser* browser, WindowOpenDisposition disposition);

}  // namespace chrome_cleaner_util

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_NAVIGATION_UTIL_WIN_H_
