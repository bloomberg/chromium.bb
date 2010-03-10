// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run.h"

#include "app/resource_bundle.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
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
                                        const FilePath& master_prefs_path,
                                        MasterPrefs* out_prefs) {
  DCHECK(!user_data_dir.empty());
  FilePath master_prefs = master_prefs_path;
  if (master_prefs.empty()) {
    // The default location of the master prefs is next to the chrome binary.
    if (!PathService::Get(base::DIR_EXE, &master_prefs))
      return true;
    master_prefs =
        master_prefs.AppendASCII(installer_util::kDefaultMasterPrefs);
  }

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
  FirstRun::SetShowFirstRunBubblePref();

  if (installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroShowWelcomePage, &value) &&
      value)
    FirstRun::SetShowWelcomePagePref();

  return false;
}

