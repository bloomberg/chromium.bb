// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service.h"

#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"

namespace {

void SendAppListAppLaunch(int count) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Apps.AppListDailyAppLaunches", count, 1, 1000, 50);
}

void SendAppListLaunch(int count) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Apps.AppListDailyLaunches", count, 1, 1000, 50);
}

bool SendDailyEventFrequency(
    const char* last_ping_pref,
    const char* count_pref,
    void (*send_callback)(int count)) {
  PrefService* local_state = g_browser_process->local_state();

  base::Time now = base::Time::Now();
  base::Time last = base::Time::FromInternalValue(local_state->GetInt64(
      last_ping_pref));
  int days = (now - last).InDays();
  if (days > 0) {
    send_callback(local_state->GetInteger(count_pref));
    local_state->SetInt64(
        last_ping_pref,
        (last + base::TimeDelta::FromDays(days)).ToInternalValue());
    local_state->SetInteger(count_pref, 0);
    return true;
  }
  return false;
}

void RecordDailyEventFrequency(
    const char* last_ping_pref,
    const char* count_pref,
    void (*send_callback)(int count)) {
  PrefService* local_state = g_browser_process->local_state();

  int count = local_state->GetInteger(count_pref);
  local_state->SetInteger(count_pref, count + 1);
  if (SendDailyEventFrequency(last_ping_pref, count_pref, send_callback)) {
    local_state->SetInteger(count_pref, 1);
  }
}

}  // namespace

// static
void AppListService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kLastAppListLaunchPing, 0);
  registry->RegisterIntegerPref(prefs::kAppListLaunchCount, 0);
  registry->RegisterInt64Pref(prefs::kLastAppListAppLaunchPing, 0);
  registry->RegisterIntegerPref(prefs::kAppListAppLaunchCount, 0);
  registry->RegisterStringPref(prefs::kAppListProfile, "");
#if defined(OS_WIN)
  registry->RegisterBooleanPref(prefs::kRestartWithAppList, false);
#endif
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

// static
void AppListService::RecordAppListLaunch() {
  RecordDailyEventFrequency(prefs::kLastAppListLaunchPing,
                            prefs::kAppListLaunchCount,
                            &SendAppListLaunch);
}

// static
void AppListService::RecordAppListAppLaunch() {
  RecordDailyEventFrequency(prefs::kLastAppListAppLaunchPing,
                            prefs::kAppListAppLaunchCount,
                            &SendAppListAppLaunch);
}

// static
void AppListService::SendAppListStats() {
  SendDailyEventFrequency(prefs::kLastAppListLaunchPing,
                          prefs::kAppListLaunchCount,
                          &SendAppListLaunch);
  SendDailyEventFrequency(prefs::kLastAppListAppLaunchPing,
                          prefs::kAppListAppLaunchCount,
                          &SendAppListAppLaunch);
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

AppListControllerDelegate* AppListService::CreateControllerDelegate() {
  return NULL;
}
