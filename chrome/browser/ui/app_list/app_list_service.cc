// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"

// static
void AppListService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kAppListProfile, "");
}

void AppListService::Init(Profile* initial_profile) {}

base::FilePath AppListService::GetAppListProfilePath(
    const base::FilePath& user_data_dir) {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  std::string app_list_profile;
  if (local_state->HasPrefPath(prefs::kAppListProfile))
    app_list_profile = local_state->GetString(prefs::kAppListProfile);

  // If the user has no profile preference for the app launcher, default to the
  // last browser profile used.
  if (app_list_profile.empty() &&
      local_state->HasPrefPath(prefs::kProfileLastUsed))
    app_list_profile = local_state->GetString(prefs::kProfileLastUsed);

  std::string profile_path = app_list_profile.empty() ?
      chrome::kInitialProfile :
      app_list_profile;

  return user_data_dir.AppendASCII(profile_path);
}

void AppListService::ShowAppList(Profile* profile) {}

void AppListService::DismissAppList() {}

void AppListService::SetAppListProfile(
    const base::FilePath& profile_file_path) {}

Profile* AppListService::GetCurrentAppListProfile() { return NULL; }

bool AppListService::IsAppListVisible() const { return false; }

void AppListService::OnProfileAdded(const base::FilePath& profilePath) {}

void AppListService::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {}

void AppListService::OnProfileWasRemoved(const base::FilePath& profile_path,
                                         const string16& profile_name) {}

void AppListService::OnProfileNameChanged(const base::FilePath& profile_path,
                                          const string16& profile_name) {}

void AppListService::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {}
