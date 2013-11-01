// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_FIRST_RUN_INTERNAL_H_
#define CHROME_BROWSER_FIRST_RUN_FIRST_RUN_INTERNAL_H_

class MasterPrefs;
class Profile;

namespace base {
class FilePath;
}

namespace installer {
class MasterPreferences;
}

namespace first_run {

namespace internal {

enum FirstRunState {
  FIRST_RUN_UNKNOWN,  // The state is not tested or set yet.
  FIRST_RUN_TRUE,
  FIRST_RUN_FALSE,
  FIRST_RUN_CANCEL,  // This shouldn't be considered first run but the sentinel
                     // should be created anyways.
};

// This variable should only be accessed through IsChromeFirstRun().
extern FirstRunState first_run_;

// Generates an initial user preference file in |user_data_dir| using the data
// in |master_prefs|.
bool GeneratePrefFile(const base::FilePath& user_data_dir,
                      const installer::MasterPreferences& master_prefs);

// Sets up master preferences by preferences passed by installer.
void SetupMasterPrefsFromInstallPrefs(
    const installer::MasterPreferences& install_prefs,
    MasterPrefs* out_prefs);

void SetDefaultBrowser(installer::MasterPreferences* install_prefs);

// Creates the sentinel file that signals that chrome has been configured.
bool CreateSentinel();

// -- Platform-specific functions --

void DoPostImportPlatformSpecificTasks(Profile* profile);

// Gives the full path to the sentinel file. The file might not exist.
// This function has a common implementation on OS_POSIX and a windows specific
// implementation.
bool GetFirstRunSentinelFilePath(base::FilePath* path);

// This function has a common implementationin for all non-linux platforms, and
// a linux specific implementation.
bool IsOrganicFirstRun();

// Shows the EULA dialog if required. Returns true if the EULA is accepted,
// returns false if the EULA has not been accepted, in which case the browser
// should exit.
bool ShowPostInstallEULAIfNeeded(installer::MasterPreferences* install_prefs);

// Returns the path for the master preferences file.
base::FilePath MasterPrefsPath();

}  // namespace internal
}  // namespace first_run

#endif  // CHROME_BROWSER_FIRST_RUN_FIRST_RUN_INTERNAL_H_
