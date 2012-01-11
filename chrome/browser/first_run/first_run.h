// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_
#define CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/native_widget_types.h"

class CommandLine;
class FilePath;
class GURL;
class ImporterHost;
class ImporterList;
class Profile;
class ProcessSingleton;
class TemplateURLService;

// TODO(jennyz): All FirstRun class code will be refactored to first_run
// namespace progressively with several changelists to be landed. Therefore,
// we keep first_run namespace and FirstRun in the same file temporarily.

// This namespace contains the chrome first-run installation actions needed to
// fully test the custom installer. It also contains the opposite actions to
// execute during uninstall. When the first run UI is ready we won't
// do the actions unconditionally. Currently the only action is to create a
// desktop shortcut.
//
// The way we detect first-run is by looking at a 'sentinel' file.
// If it does not exist we understand that we need to do the first time
// install work for this user. After that the sentinel file is created.
namespace first_run {

// Returns true if this is the first time chrome is run for this user.
bool IsChromeFirstRun();

// Creates the sentinel file that signals that chrome has been configured.
bool CreateSentinel();

// Removes the sentinel file created in ConfigDone(). Returns false if the
// sentinel file could not be removed.
bool RemoveSentinel();

// Sets the kShouldShowFirstRunBubble local state pref so that the browser
// shows the bubble once the main message loop gets going (or refrains from
// showing the bubble, if |show_bubble| is false). Returns false if the pref
// could not be set. This function can be called multiple times, but only the
// initial call will actually set the preference.
bool SetShowFirstRunBubblePref(bool show_bubble);

// Sets the kShouldUseMinimalFirstRunBubble local state pref so that the
// browser shows the minimal first run bubble once the main message loop
// gets going. Returns false if the pref could not be set.
bool SetMinimalFirstRunBubblePref();

// Sets the kShouldShowWelcomePage local state pref so that the browser
// loads the welcome tab once the message loop gets going. Returns false
// if the pref could not be set.
bool SetShowWelcomePagePref();

// Sets the kAutofillPersonalDataManagerFirstRun local state pref so that the
// browser loads PersonalDataManager once the main message loop gets going.
// Returns false if the pref could not be set.
bool SetPersonalDataManagerFirstRunPref();

// Sets the kShouldUseOEMFirstRunBubble local state pref so that the
// browser shows the OEM first run bubble once the main message loop
// gets going. Returns false if the pref could not be set.
bool SetOEMFirstRunBubblePref();

// Whether the search engine selection dialog should be shown on first run.
bool ShouldShowSearchEngineSelector(const TemplateURLService* model);

// -- Platform-specific functions --

// Automatically import history and home page (and search engine, if
// ShouldShowSearchEngineDialog is true).
void AutoImport(Profile* profile,
                bool homepage_defined,
                int import_items,
                int dont_import_items,
                bool search_engine_experiment,
                bool randomize_search_engine_experiment,
                bool make_chrome_default,
                ProcessSingleton* process_singleton);

// Imports bookmarks and/or browser items (depending on platform support)
// in this process. This function is paired with first_run::ImportSettings().
// This function might or might not show a visible UI depending on the
// cmdline parameters.
int ImportNow(Profile* profile, const CommandLine& cmdline);

// Returns the path for the master preferences file.
FilePath MasterPrefsPath();

}  // namespace first_run

// This class contains the chrome first-run installation actions needed to
// fully test the custom installer. It also contains the opposite actions to
// execute during uninstall. When the first run UI is ready we won't
// do the actions unconditionally. Currently the only action is to create a
// desktop shortcut.
//
// The way we detect first-run is by looking at a 'sentinel' file.
// If it does not exist we understand that we need to do the first time
// install work for this user. After that the sentinel file is created.
class FirstRun {
 public:
  // There are three types of possible first run bubbles:
  enum BubbleType {
    LARGE_BUBBLE,      // The normal bubble, with search engine choice
    OEM_BUBBLE,        // Smaller bubble for OEM builds
    MINIMAL_BUBBLE     // Minimal bubble shown after search engine dialog
  };

  // See ProcessMasterPreferences for more info about this structure.
  struct MasterPrefs {
    MasterPrefs();
    ~MasterPrefs();

    int ping_delay;
    bool homepage_defined;
    int do_import_items;
    int dont_import_items;
    bool run_search_engine_experiment;
    bool randomize_search_engine_experiment;
    bool make_chrome_default;
    std::vector<GURL> new_tabs;
    std::vector<GURL> bookmarks;
  };

  // The master preferences is a JSON file with the same entries as the
  // 'Default\Preferences' file. This function locates this file from a standard
  // location and processes it so it becomes the default preferences in the
  // profile pointed to by |user_data_dir|. After processing the file, the
  // function returns true if and only if showing the first run dialog is
  // needed. The detailed settings in the preference file are reported via
  // |preference_details|.
  //
  // This function destroys any existing prefs file and it is meant to be
  // invoked only on first run.
  //
  // See chrome/installer/util/master_preferences.h for a description of
  // 'master_preferences' file.
  static bool ProcessMasterPreferences(const FilePath& user_data_dir,
                                       MasterPrefs* out_prefs);

 private:
  friend class FirstRunTest;
  FRIEND_TEST_ALL_PREFIXES(Toolbar5ImporterTest, BookmarkParse);

  // -- Platform-specific functions --

#if defined(OS_WIN)
  // Writes the EULA to a temporary file, returned in |*eula_path|, and returns
  // true if successful.
  static bool WriteEULAtoTempFile(FilePath* eula_path);

  // Launches the setup exe with the given parameter/value on the command-line,
  // waits for its termination, returns its exit code in |*ret_code|, and
  // returns true if the exit code is valid.
  static bool LaunchSetupWithParam(const std::string& param,
                                   const std::wstring& value,
                                   int* ret_code);

  // Installs a task to do an extensions update check once the extensions system
  // is running.
  static void DoDelayedInstallExtensions();

#endif

  // This class is for scoping purposes.
  DISALLOW_IMPLICIT_CONSTRUCTORS(FirstRun);
};

#endif  // CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_
