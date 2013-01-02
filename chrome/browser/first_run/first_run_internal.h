// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_FIRST_RUN_INTERNAL_H_
#define CHROME_BROWSER_FIRST_RUN_FIRST_RUN_INTERNAL_H_

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

namespace installer {
class MasterPreferences;
}

namespace first_run {
namespace internal {

enum FirstRunState {
  FIRST_RUN_UNKNOWN,  // The state is not tested or set yet.
  FIRST_RUN_TRUE,
  FIRST_RUN_FALSE
};

// This variable should only be accessed through IsChromeFirstRun().
extern FirstRunState first_run_;

// Loads master preferences from the master preference file into the installer
// master preferences. Passes the master preference file path out in
// master_prefs_path. Returns the pointer to installer::MasterPreferences object
// if successful; otherwise, returns NULL.
installer::MasterPreferences* LoadMasterPrefs(FilePath* master_prefs_path);

// Copies user preference file to master preference file. Returns true if
// successful.
bool CopyPrefFile(const FilePath& user_data_dir,
                  const FilePath& master_prefs_path);

// Sets up master preferences by preferences passed by installer.
void SetupMasterPrefsFromInstallPrefs(
    MasterPrefs* out_prefs,
    installer::MasterPreferences* install_prefs);

void SetShowWelcomePagePrefIfNeeded(
    installer::MasterPreferences* install_prefs);

void SetDefaultBrowser(installer::MasterPreferences* install_prefs);

// Returns true if first run ui should be skipped, which is the case that
// skip_first_run_ui setting is set to true. In the case the setting is
// not found or specified, it returns false by default.
bool SkipFirstRunUI(installer::MasterPreferences* install_prefs);

// Sets ping_delay.
void SetRLZPref(first_run::MasterPrefs* out_prefs,
                installer::MasterPreferences* install_prefs);

// -- Platform-specific functions --

void DoPostImportPlatformSpecificTasks();

// Gives the full path to the sentinel file. The file might not exist.
// This function has a common implementation on OS_POSIX and a windows specific
// implementation.
bool GetFirstRunSentinelFilePath(FilePath* path);

// This function has a common implementationin for all non-linux platforms, and
// a linux specific implementation.
bool IsOrganicFirstRun();

// Imports settings. This may be done in a separate process depending on the
// platform, but it will always block until done. The return value indicates
// success.
// This functions has a common implementation for OS_POSIX, and a
// windows specific implementation.
bool ImportSettings(Profile* profile,
                    scoped_refptr<ImporterHost> importer_host,
                    scoped_refptr<ImporterList> importer_list,
                    int items_to_import);

// Sets import preferences and launch the import process.
void SetImportPreferencesAndLaunchImport(
    MasterPrefs* out_prefs,
    installer::MasterPreferences* install_prefs);

int ImportBookmarkFromFileIfNeeded(Profile* profile,
                                   const CommandLine& cmdline);

#if !defined(OS_WIN)
bool ImportBookmarks(const FilePath& import_bookmarks_path);
#endif

// Shows the EULA dialog if required. Returns true if the EULA is accepted,
// returns false if the EULA has not been accepted, in which case the browser
// should exit.
bool ShowPostInstallEULAIfNeeded(installer::MasterPreferences* install_prefs);

}  // namespace internal
}  // namespace first_run

#endif  // CHROME_BROWSER_FIRST_RUN_FIRST_RUN_INTERNAL_H_
