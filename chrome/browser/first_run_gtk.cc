// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run.h"

#include "app/app_switches.h"
#include "app/resource_bundle.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "chrome/browser/gtk/first_run_dialog.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/util_constants.h"
#include "googleurl/src/gurl.h"

bool OpenFirstRunDialog(Profile* profile, bool homepage_defined,
                        int import_items,
                        int dont_import_items,
                        bool search_engine_experiment,
                        bool randomize_search_engine_experiment,
                        ProcessSingleton* process_singleton) {
  return FirstRunDialog::Show(profile, process_singleton);
}

FilePath GetDefaultPrefFilePath(bool create_profile_dir,
                                const FilePath& user_data_dir) {
  FilePath default_pref_dir =
      ProfileManager::GetDefaultProfileDir(user_data_dir);
  if (create_profile_dir) {
    if (!file_util::PathExists(default_pref_dir)) {
      if (!file_util::CreateDirectory(default_pref_dir))
        return FilePath();
    }
  }
  return ProfileManager::GetProfilePrefsPath(default_pref_dir);
}

bool FirstRun::ProcessMasterPreferences(const FilePath& user_data_dir,
                                        MasterPrefs* out_prefs) {
  DCHECK(!user_data_dir.empty());

  // The standard location of the master prefs is next to the chrome binary.
  FilePath master_prefs;
  if (!PathService::Get(base::DIR_EXE, &master_prefs))
    return true;
  master_prefs = master_prefs.AppendASCII(installer_util::kDefaultMasterPrefs);

  scoped_ptr<DictionaryValue> prefs(
      installer_util::ParseDistributionPreferences(master_prefs));
  if (!prefs.get())
    return true;

  out_prefs->new_tabs = installer_util::GetFirstRunTabs(prefs.get());

  std::string not_used;
  out_prefs->homepage_defined = prefs->GetString(prefs::kHomePage, &not_used);

  bool value = false;
  if (installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kAltFirstRunBubble, &value) && value)
    FirstRun::SetOEMFirstRunBubblePref();

  FilePath user_prefs = GetDefaultPrefFilePath(true, user_data_dir);
  if (user_prefs.empty())
    return true;

  // The master prefs are regular prefs so we can just copy the file
  // to the default place and they just work.
  if (!file_util::CopyFile(master_prefs, user_prefs))
    return true;

  // Note we are skipping all other master preferences if skip-first-run-ui
  // is *not* specified.
  if (!installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroSkipFirstRunPref, &value) ||
      !value)
    return true;

  // From here on we won't show first run so we need to do the work to set the
  // required state given that FirstRunView is not going to be called.
  FirstRun::SetShowFirstRunBubblePref(true);

  if (installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroShowWelcomePage, &value) &&
      value)
    FirstRun::SetShowWelcomePagePref();

  // We need to be able to create the first run sentinel or else we cannot
  // proceed because ImportSettings will launch the importer process which
  // would end up here if the sentinel is not present.
  if (!FirstRun::CreateSentinel())
    return false;

  std::wstring import_bookmarks_path;
  installer_util::GetDistroStringPreference(prefs.get(),
      installer_util::master_preferences::kDistroImportBookmarksFromFilePref,
      &import_bookmarks_path);

  if (!import_bookmarks_path.empty()) {
    // There are bookmarks to import from a file.
    if (!FirstRun::ImportBookmarks(import_bookmarks_path)) {
      LOG(WARNING) << "silent bookmark import failed";
    }
  }
  return false;
}

//  TODO(port): This is just a piece of the silent import functionality from
// ImportSettings for Windows.  It would be nice to get the rest of it ported.
bool FirstRun::ImportBookmarks(const std::wstring& import_bookmarks_path) {
  const CommandLine& cmdline = *CommandLine::ForCurrentProcess();
  CommandLine import_cmd(cmdline.GetProgram());

  // Propagate user data directory switch.
  if (cmdline.HasSwitch(switches::kUserDataDir)) {
    import_cmd.AppendSwitchWithValue(
        switches::kUserDataDir,
        cmdline.GetSwitchValueASCII(switches::kUserDataDir));
  }
  // Since ImportSettings is called before the local state is stored on disk
  // we pass the language as an argument. GetApplicationLocale checks the
  // current command line as fallback.
  import_cmd.AppendSwitchWithValue(
      switches::kLang,
      ASCIIToWide(g_browser_process->GetApplicationLocale()));

  import_cmd.CommandLine::AppendSwitchWithValue(
      switches::kImportFromFile, import_bookmarks_path);
  // Time to launch the process that is going to do the import. We'll wait
  // for the process to return.
  return base::LaunchApp(import_cmd, true, false, NULL);
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
CommandLine* Upgrade::new_command_line_ = NULL;
double Upgrade::saved_last_modified_time_of_exe_ = 0;

// static
bool Upgrade::IsUpdatePendingRestart() {
  return saved_last_modified_time_of_exe_ !=
      Upgrade::GetLastModifiedTimeOfExe();
}

// static
void Upgrade::SaveLastModifiedTimeOfExe() {
  saved_last_modified_time_of_exe_ = Upgrade::GetLastModifiedTimeOfExe();
}

// static
bool Upgrade::RelaunchChromeBrowser(const CommandLine& command_line) {
  return base::LaunchApp(command_line, false, false, NULL);
}

// static
double Upgrade::GetLastModifiedTimeOfExe() {
  FilePath exe_file_path;
  if (!PathService::Get(base::FILE_EXE, &exe_file_path)) {
    LOG(WARNING) << "Failed to get FilePath object for FILE_EXE.";
    return saved_last_modified_time_of_exe_;
  }
  file_util::FileInfo exe_file_info;
  if (!file_util::GetFileInfo(exe_file_path, &exe_file_info)) {
    LOG(WARNING) << "Failed to get FileInfo object for FILE_EXE - "
                 << exe_file_path.value();
    return saved_last_modified_time_of_exe_;
  }
  return exe_file_info.last_modified.ToDoubleT();
}
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)
