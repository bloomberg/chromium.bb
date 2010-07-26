// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains functions processing master preference file used by
// setup and first run.

#ifndef CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_H_
#define CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_H_
#pragma once

#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "chrome/installer/util/master_preferences_constants.h"

namespace installer_util {

// This is the default name for the master preferences file used to pre-set
// values in the user profile at first run.
const char kDefaultMasterPrefs[] = "master_preferences";

// Parse command line and read master preferences, if present, to get
// distribution related install options. Merge them if any command line
// options present (command line value takes precedence).
DictionaryValue* GetInstallPreferences(const CommandLine& cmd_line);

// Gets the value of given boolean preference |name| from |prefs| dictionary
// which is assumed to contain a dictionary named "distribution". Returns
// true if the value is read successfully, otherwise false.
bool GetDistroBooleanPreference(const DictionaryValue* prefs,
                                const std::wstring& name,
                                bool* value);

// This function gets value of a string preference from master
// preferences. Returns true if the value is read successfully, otherwise false.
bool GetDistroStringPreference(const DictionaryValue* prefs,
                               const std::wstring& name,
                               std::wstring* value);

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
//      "import_bookmarks_from_file": "c:\\path",
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
// This function retuns the list as a vector of GURLs.  If the master
// preferences file does not contain such list the vector is empty.
std::vector<GURL> GetFirstRunTabs(const DictionaryValue* prefs);

// Sets the value of given boolean preference |name| in "distribution"
// dictionary inside |prefs| dictionary.
bool SetDistroBooleanPreference(DictionaryValue* prefs,
                                const std::wstring& name,
                                bool value);

// The master preferences can also contain a regular extensions
// preference block. If so, the extensions referenced there will be
// installed during the first run experience.
// An extension can go in the master prefs needs just the basic
// elements such as:
//   1- An extension entry under settings, assigned by the gallery
//   2- The "location" : 1 entry
//   3- A minimal "manifest" block with key, name, permissions, update url
//      and version. The version needs to be lower than the version of
//      the extension that is hosted in the gallery.
//   4- The "path" entry with the version as last component
//   5- The "state" : 1 entry
//
// The following is an example of a master pref file that installs
// Google XYZ:
//
//  {
//     "extensions": {
//        "settings": {
//           "ppflmjolhbonpkbkooiamcnenbmbjcbb": {
//              "location": 1,
//              "manifest": {
//                 "key": "MIGfMA0GCSqGSIb3DQEBAQUAA4<rest of key ommited>",
//                 "name": "Google XYZ (Installing...)",
//                 "permissions": [ "tabs", "http://xyz.google.com/" ],
//                 "update_url": "http://fixme.com/fixme/fixme/crx",
//                 "version": "0.0"
//              },
//              "path": "ppflmjolhbonpkbkooiamcnenbmbjcbb\\0.0",
//              "state": 1
//           }
//        }
//     }
//  }
//
bool HasExtensionsBlock(const DictionaryValue* prefs,
                        DictionaryValue** extensions);

}  // namespace installer_util

#endif  // CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_H_
