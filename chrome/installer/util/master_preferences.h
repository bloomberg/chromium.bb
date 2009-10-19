// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains functions processing master preference file used by
// setup and first run.

#ifndef CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_H_
#define CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_H_

#include "base/file_path.h"
#include "base/values.h"

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
// Integer. Icon index from chrome.exe to use for shortcuts.
extern const wchar_t kChromeShortcutIconIndex[];
// Boolean. Create Desktop and QuickLaunch shortcuts. Cmd line override present.
extern const wchar_t kCreateAllShortcuts[];
// Boolean pref that triggers silent import of the default browser bookmarks.
extern const wchar_t kDistroImportBookmarksPref[];
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
// Boolean. Show EULA dialog before install.
extern const wchar_t kRequireEula[];
// Boolean. Install Chrome to system wise location. Cmd line override present.
extern const wchar_t kSystemLevel[];
// Boolean. Run installer in verbose mode. Cmd line override present.
extern const wchar_t kVerboseLogging[];
}

// This is the default name for the master preferences file used to pre-set
// values in the user profile at first run.
const wchar_t kDefaultMasterPrefs[] = L"master_preferences";

// Gets the value of given boolean preference |name| from |prefs| dictionary
// which is assumed to contain a dictionary named "distribution". Returns
// true if the value is read successfully, otherwise false.
bool GetDistroBooleanPreference(const DictionaryValue* prefs,
                                const std::wstring& name,
                                bool* value);

// This function gets value of an integer preference from master
// preferences. Returns true if the value is read successfully, otherwise false.
bool GetDistroIntegerPreference(const DictionaryValue* prefs,
                                const std::wstring& name,
                                int* value);

// The master preferences is a JSON file with the same entries as the
// 'Default\Preferences' file. This function parses the distribution
// section of the preferences file.
//
// A prototypical 'master_preferences' file looks like this:
//
// {
//   "distribution": {
//      "alternate_shortcut_text": false,
//      "oem_bubble": false,
//      "chrome_shortcut_icon_index": 0,
//      "create_all_shortcuts": true,
//      "import_bookmarks": false,
//      "import_history": false,
//      "import_home_page": false,
//      "import_search_engine": true,
//      "ping_delay": 40,
//      "show_welcome_page": true,
//      "skip_first_run_ui": true,
//      "do_not_launch_chrome": false,
//      "make_chrome_default": false,
//      "make_chrome_default_for_user": true,
//      "require_eula": true,
//      "system_level": false,
//      "verbose_logging": true
//   },
//   "browser": {
//      "show_home_button": true
//   },
//   "bookmark_bar": {
//      "show_on_all_tabs": true
//   },
//   "first_run_tabs": [
//      "http://gmail.com",
//      "https://igoogle.com"
//   ],
//   "homepage": "http://example.org",
//   "homepage_is_newtabpage": false
// }
//
// A reserved "distribution" entry in the file is used to group related
// installation properties. This entry will be ignored at other times.
// This function parses the 'distribution' entry and returns a combination
// of MasterPrefResult.
DictionaryValue* ParseDistributionPreferences(
    const FilePath& master_prefs_path);

// As part of the master preferences an optional section indicates the tabs
// to open during first run. An example is the following:
//
//  {
//    "first_run_tabs": [
//       "http://google.com/f1",
//       "https://google.com/f2"
//    ]
//  }
//
// Note that the entries are usually urls but they don't have to.
//
// This function retuns the list as a vector of strings. If the master
// preferences file does not contain such list the vector is empty.
std::vector<std::wstring> GetFirstRunTabs(const DictionaryValue* prefs);

// Sets the value of given boolean preference |name| in "distribution"
// dictionary inside |prefs| dictionary.
bool SetDistroBooleanPreference(DictionaryValue* prefs,
                                const std::wstring& name,
                                bool value);
}

#endif  // CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_H_
