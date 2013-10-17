// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profiles_state.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/prefs/pref_registry_simple.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

namespace profiles {

bool IsMultipleProfilesEnabled() {
#if defined(OS_ANDROID)
  return false;
#endif
#if defined(OS_CHROMEOS)
  return chromeos::UserManager::IsMultipleProfilesAllowed();
#endif

  return true;
}

bool IsNewProfileManagementEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kNewProfileManagement);
}

base::FilePath GetDefaultProfileDir(const base::FilePath& user_data_dir) {
  base::FilePath default_profile_dir(user_data_dir);
  default_profile_dir =
      default_profile_dir.AppendASCII(chrome::kInitialProfile);
  return default_profile_dir;
}

base::FilePath GetProfilePrefsPath(
    const base::FilePath &profile_dir) {
  base::FilePath default_prefs_path(profile_dir);
  default_prefs_path = default_prefs_path.Append(chrome::kPreferencesFilename);
  return default_prefs_path;
}

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kProfileLastUsed, std::string());
  registry->RegisterIntegerPref(prefs::kProfilesNumCreated, 1);
  registry->RegisterListPref(prefs::kProfilesLastActive);
}

string16 GetActiveProfileDisplayName(Browser* browser) {
  string16 profile_name;
  Profile* profile = browser->profile();

  if (profile->IsGuestSession()) {
    profile_name = l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);
  } else {
    ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    size_t index = cache.GetIndexOfProfileWithPath(profile->GetPath());
    if (index != std::string::npos)
      profile_name = cache.GetNameOfProfileAtIndex(index);
  }
  return profile_name;
}

}  // namespace profiles
