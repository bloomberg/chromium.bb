// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the constants used to process master_preferences files
// used by setup and first run.

#ifndef CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_CONSTANTS_H_
#define CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_CONSTANTS_H_
#pragma once

namespace installer_util {
namespace master_preferences {
// All the preferences below are expected to be inside the JSON "distribution"
// block. Some of them also have equivalent command line option. If same option
// is specified in master preference as well as command line, the commnd line
// value takes precedence.

// Boolean. Use alternate text for the shortcut. Cmd line override present.
extern const wchar_t kAltShortcutText[];
// Boolean. Use alternate smaller first run info bubble.
extern const wchar_t kAltFirstRunBubble[];
// Boolean. This is to be a Chrome Frame install.
extern const wchar_t kChromeFrame[];
// Integer. Icon index from chrome.exe to use for shortcuts.
extern const wchar_t kChromeShortcutIconIndex[];
// Boolean. Create Desktop and QuickLaunch shortcuts. Cmd line override present.
extern const wchar_t kCreateAllShortcuts[];
// Boolean pref that triggers silent import of the default browser bookmarks.
extern const wchar_t kDistroImportBookmarksPref[];
// String pref that triggers silent import of bookmarks from the html file at
// given path.
extern const wchar_t kDistroImportBookmarksFromFilePref[];
// Boolean pref that triggers silent import of the default browser history.
extern const wchar_t kDistroImportHistoryPref[];
// Boolean pref that triggers silent import of the default browser homepage.
extern const wchar_t kDistroImportHomePagePref[];
// Boolean pref that triggers silent import of the default search engine.
extern const wchar_t kDistroImportSearchPref[];
// Integer. RLZ ping delay in seconds.
extern const wchar_t kDistroPingDelay[];
// Boolean pref that triggers loading the welcome page.
extern const wchar_t kDistroShowWelcomePage[];
// Boolean pref that triggers skipping the first run dialogs.
extern const wchar_t kDistroSkipFirstRunPref[];
// Boolean. Do not show first run bubble, even if it would otherwise be shown.
extern const wchar_t kDistroSuppressFirstRunBubble[];
// Boolean. Do not create Chrome desktop shortcuts. Cmd line override present.
extern const wchar_t kDoNotCreateShortcuts[];
// Boolean. Do not launch Chrome after first install. Cmd line override present.
extern const wchar_t kDoNotLaunchChrome[];
// Boolean. Do not register with Google Update to have Chrome launched after
// install. Cmd line override present.
extern const wchar_t kDoNotRegisterForUpdateLaunch[];
// Boolean. Register Chrome as default browser. Cmd line override present.
extern const wchar_t kMakeChromeDefault[];
// Boolean. Register Chrome as default browser for the current user.
extern const wchar_t kMakeChromeDefaultForUser[];
// Boolean. Expect to be run by an MSI installer. Cmd line override present.
extern const wchar_t kMsi[];
// Boolean. Show EULA dialog before install.
extern const wchar_t kRequireEula[];
// Boolean. Use experimental search engine selection dialog.
extern const wchar_t kSearchEngineExperimentPref[];
// Boolean. Randomize logos in experimental search engine selection dialog.
extern const wchar_t kSearchEngineExperimentRandomizePref[];
// Boolean. Install Chrome to system wise location. Cmd line override present.
extern const wchar_t kSystemLevel[];
// Boolean. Run installer in verbose mode. Cmd line override present.
extern const wchar_t kVerboseLogging[];
// Name of the block that contains the extensions on the master preferences.
extern const wchar_t kExtensionsBlock[];
}
}

#endif  // CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_CONSTANTS_H_
