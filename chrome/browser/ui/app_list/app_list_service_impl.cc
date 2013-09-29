// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_impl.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

namespace {

void SendAppListAppLaunch(int count) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Apps.AppListDailyAppLaunches", count, 1, 1000, 50);
  if (count > 0)
    UMA_HISTOGRAM_ENUMERATION("Apps.AppListHasLaunchedAppToday", 1, 2);
}

void SendAppListLaunch(int count) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Apps.AppListDailyLaunches", count, 1, 1000, 50);
  if (count > 0)
    UMA_HISTOGRAM_ENUMERATION("Apps.AppListHasLaunchedAppListToday", 1, 2);
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

bool HasAppListEnabledPreference() {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->GetBoolean(prefs::kAppLauncherHasBeenEnabled);
}

void SetAppListEnabledPreference(bool enabled) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kAppLauncherHasBeenEnabled, enabled);
}

}  // namespace

// static
void AppListServiceImpl::RecordAppListLaunch() {
  RecordDailyEventFrequency(prefs::kLastAppListLaunchPing,
                            prefs::kAppListLaunchCount,
                            &SendAppListLaunch);
}

// static
void AppListServiceImpl::RecordAppListAppLaunch() {
  RecordDailyEventFrequency(prefs::kLastAppListAppLaunchPing,
                            prefs::kAppListAppLaunchCount,
                            &SendAppListAppLaunch);
}

// static
void AppListServiceImpl::SendAppListStats() {
  if (!g_browser_process || g_browser_process->IsShuttingDown())
    return;

  SendDailyEventFrequency(prefs::kLastAppListLaunchPing,
                          prefs::kAppListLaunchCount,
                          &SendAppListLaunch);
  SendDailyEventFrequency(prefs::kLastAppListAppLaunchPing,
                          prefs::kAppListAppLaunchCount,
                          &SendAppListAppLaunch);
}

AppListServiceImpl::AppListServiceImpl()
    : profile_(NULL),
      profile_load_sequence_id_(0),
      pending_profile_loads_(0),
      weak_factory_(this),
      profile_loader_(g_browser_process->profile_manager()) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager->GetProfileInfoCache().AddObserver(this);
}

AppListServiceImpl::~AppListServiceImpl() {}

void AppListServiceImpl::SetAppListNextPaintCallback(
    const base::Closure& callback) {}

void AppListServiceImpl::HandleFirstRun() {}

void AppListServiceImpl::Init(Profile* initial_profile) {}

base::FilePath AppListServiceImpl::GetProfilePath(
    const base::FilePath& user_data_dir) {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  std::string app_list_profile;
  if (local_state->HasPrefPath(prefs::kAppListProfile))
    app_list_profile = local_state->GetString(prefs::kAppListProfile);

  // If the user has no profile preference for the app launcher, default to the
  // last browser profile used.
  if (app_list_profile.empty() &&
      local_state->HasPrefPath(prefs::kProfileLastUsed)) {
    app_list_profile = local_state->GetString(prefs::kProfileLastUsed);
  }

  // If there is no last used profile recorded, use the initial profile.
  if (app_list_profile.empty())
    app_list_profile = chrome::kInitialProfile;

  return user_data_dir.AppendASCII(app_list_profile);
}

void AppListServiceImpl::SetProfilePath(const base::FilePath& profile_path) {
  // Ensure we don't set the pref to a managed user's profile path.
  ProfileInfoCache& profile_info =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = profile_info.GetIndexOfProfileWithPath(profile_path);
  if (profile_info.ProfileIsManagedAtIndex(profile_index))
    return;

  g_browser_process->local_state()->SetString(
      prefs::kAppListProfile,
      profile_path.BaseName().MaybeAsASCII());
}

void AppListServiceImpl::CreateShortcut() {}

// We need to watch for profile removal to keep kAppListProfile updated.
void AppListServiceImpl::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
  // If the profile the app list uses just got deleted, reset it to the last
  // used profile.
  PrefService* local_state = g_browser_process->local_state();
  std::string app_list_last_profile = local_state->GetString(
      prefs::kAppListProfile);
  if (profile_path.BaseName().MaybeAsASCII() == app_list_last_profile) {
    local_state->SetString(prefs::kAppListProfile,
        local_state->GetString(prefs::kProfileLastUsed));
  }
}

void AppListServiceImpl::Show() {
  profile_loader_.LoadProfileInvalidatingOtherLoads(
      GetProfilePath(g_browser_process->profile_manager()->user_data_dir()),
      base::Bind(&AppListServiceImpl::ShowForProfile,
                 weak_factory_.GetWeakPtr()));
}

void AppListServiceImpl::EnableAppList(Profile* initial_profile) {
  SetProfilePath(initial_profile->GetPath());
  if (HasAppListEnabledPreference())
    return;

  SetAppListEnabledPreference(true);
  CreateShortcut();
}

Profile* AppListServiceImpl::GetCurrentAppListProfile() {
  return profile();
}

void AppListServiceImpl::SetProfile(Profile* new_profile) {
  profile_ = new_profile;
}

void AppListServiceImpl::InvalidatePendingProfileLoads() {
  profile_loader_.InvalidatePendingProfileLoads();
}

void AppListServiceImpl::HandleCommandLineFlags(Profile* initial_profile) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableAppList))
    EnableAppList(initial_profile);

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableAppList))
    SetAppListEnabledPreference(false);

  // Send app list usage stats after a delay.
  const int kSendUsageStatsDelay = 5;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AppListServiceImpl::SendAppListStats),
      base::TimeDelta::FromSeconds(kSendUsageStatsDelay));
}
