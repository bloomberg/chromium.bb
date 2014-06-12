// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_
#define CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

class GURL;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

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

enum AutoImportState {
  AUTO_IMPORT_NONE = 0,
  AUTO_IMPORT_CALLED = 1 << 0,
  AUTO_IMPORT_PROFILE_IMPORTED = 1 << 1,
  AUTO_IMPORT_BOOKMARKS_FILE_IMPORTED = 1 << 2,
};

enum FirstRunBubbleMetric {
  FIRST_RUN_BUBBLE_SHOWN = 0,       // The search engine bubble was shown.
  FIRST_RUN_BUBBLE_CHANGE_INVOKED,  // The bubble's "Change" was invoked.
  NUM_FIRST_RUN_BUBBLE_METRICS
};

// Options for the first run bubble. The default is FIRST_RUN_BUBBLE_DONT_SHOW.
// FIRST_RUN_BUBBLE_SUPPRESS is stronger in that FIRST_RUN_BUBBLE_SHOW should
// never be set once FIRST_RUN_BUBBLE_SUPPRESS is set.
enum FirstRunBubbleOptions {
  FIRST_RUN_BUBBLE_DONT_SHOW,
  FIRST_RUN_BUBBLE_SUPPRESS,
  FIRST_RUN_BUBBLE_SHOW,
};

enum ProcessMasterPreferencesResult {
  FIRST_RUN_PROCEED = 0,  // Proceed with first run.
  EULA_EXIT_NOW,          // Should immediately exit due to EULA flow.
};

// See ProcessMasterPreferences for more info about this structure.
struct MasterPrefs {
  MasterPrefs();
  ~MasterPrefs();

  // TODO(macourteau): as part of the master preferences refactoring effort,
  // remove items from here which are being stored temporarily only to be later
  // dumped into local_state. Also see related TODO in chrome_browser_main.cc.

  int ping_delay;
  bool homepage_defined;
  int do_import_items;
  int dont_import_items;
  bool make_chrome_default_for_user;
  bool suppress_first_run_default_browser_prompt;
  std::vector<GURL> new_tabs;
  std::vector<GURL> bookmarks;
  std::string import_bookmarks_path;
  std::string variations_seed;
  std::string variations_seed_signature;
  std::string suppress_default_browser_prompt_for_version;
};

// Returns true if this is the first time chrome is run for this user.
bool IsChromeFirstRun();

#if defined(OS_MACOSX)
// Returns true if |command_line|'s switches explicitly specify that first run
// should be suppressed in the current run.
bool IsFirstRunSuppressed(const base::CommandLine& command_line);
#endif

// Creates the first run sentinel if needed. This should only be called after
// the process singleton has been grabbed by the current process
// (http://crbug.com/264694).
void CreateSentinelIfNeeded();

// Get RLZ ping delay pref name.
std::string GetPingDelayPrefName();

// Register user preferences used by the MasterPrefs structure.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Sets the kShowFirstRunBubbleOption local state pref so that the browser
// shows the bubble once the main message loop gets going (or refrains from
// showing the bubble, if |show_bubble| is not FIRST_RUN_BUBBLE_SHOW).
// Once FIRST_RUN_BUBBLE_SUPPRESS is set, no other value can be set.
// Returns false if the pref service could not be retrieved.
bool SetShowFirstRunBubblePref(FirstRunBubbleOptions show_bubble_option);

// Sets a flag that will cause ShouldShowWelcomePage to return true
// exactly once, so that the browser loads the welcome tab once the
// message loop gets going.
void SetShouldShowWelcomePage();

// Returns true if the welcome page should be shown.
//
// This will return true only once: The first time it is called after
// SetShouldShowWelcomePage() is called.
bool ShouldShowWelcomePage();

// Sets a flag that will cause ShouldDoPersonalDataManagerFirstRun()
// to return true exactly once, so that the browser loads
// PersonalDataManager once the main message loop gets going.
void SetShouldDoPersonalDataManagerFirstRun();

// Returns true if the autofill personal data manager first-run action
// should be taken.
//
// This will return true only once, the first time it is called after
// SetShouldDoPersonalDataManagerFirstRun() is called.
bool ShouldDoPersonalDataManagerFirstRun();

// Log a metric for the "FirstRun.SearchEngineBubble" histogram.
void LogFirstRunMetric(FirstRunBubbleMetric metric);

// Automatically import history and home page (and search engine, if
// ShouldShowSearchEngineDialog is true). Also imports bookmarks from file if
// |import_bookmarks_path| is not empty.
void AutoImport(Profile* profile,
                bool homepage_defined,
                int import_items,
                int dont_import_items,
                const std::string& import_bookmarks_path);

// Does remaining first run tasks. This can pop the first run consent dialog on
// linux. |make_chrome_default_for_user| is the value of
// kMakeChromeDefaultForUser in master_preferences which contributes to the
// decision of making chrome default browser in post import tasks.
void DoPostImportTasks(Profile* profile, bool make_chrome_default_for_user);

// Returns the current state of AutoImport as recorded in a bitfield formed from
// values in AutoImportState.
uint16 auto_import_state();

// Set a master preferences file path that overrides platform defaults.
void SetMasterPrefsPathForTesting(const base::FilePath& master_prefs);

// The master_preferences is a JSON file with the same entries as the
// 'Default\Preferences' file. This function locates this file from a standard
// location, processes it, and uses its content to initialize the preferences
// for the profile pointed to by |user_data_dir|. After processing the file,
// this function returns a value from the ProcessMasterPreferencesResult enum,
// indicating whether the first run flow should be shown, skipped, or whether
// the browser should exit.
//
// This function overwrites any existing Preferences file and is only meant to
// be invoked on first run.
//
// See chrome/installer/util/master_preferences.h for a description of
// 'master_preferences' file.
ProcessMasterPreferencesResult ProcessMasterPreferences(
    const base::FilePath& user_data_dir,
    MasterPrefs* out_prefs);

}  // namespace first_run

#endif  // CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_
