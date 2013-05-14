// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_impl.h"

#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

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
      weak_factory_(this) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager->GetProfileInfoCache().AddObserver(this);
}

AppListServiceImpl::~AppListServiceImpl() {}

void AppListServiceImpl::Init(Profile* initial_profile) {}

base::FilePath AppListServiceImpl::GetAppListProfilePath(
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

AppListControllerDelegate* AppListServiceImpl::CreateControllerDelegate() {
  return NULL;
}

bool AppListServiceImpl::HasCurrentView() const { return false; }

void AppListServiceImpl::DoWarmupForProfile(Profile* initial_profile) {}

void AppListServiceImpl::OnSigninStatusChanged() {}

void AppListServiceImpl::OnProfileAdded(const base::FilePath& profilePath) {}

void AppListServiceImpl::OnProfileWasRemoved(
    const base::FilePath& profile_path, const string16& profile_name) {}

void AppListServiceImpl::OnProfileNameChanged(
    const base::FilePath& profile_path, const string16& profile_name) {}

void AppListServiceImpl::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {}

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

void AppListServiceImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  OnSigninStatusChanged();
}

void AppListServiceImpl::SetAppListProfile(
    const base::FilePath& profile_file_path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetProfileByPath(profile_file_path);

  if (!profile) {
    LoadProfileAsync(profile_file_path);
    return;
  }

  ShowAppList(profile);
}

void AppListServiceImpl::ShowForSavedProfile() {
  SetAppListProfile(GetAppListProfilePath(
      g_browser_process->profile_manager()->user_data_dir()));
}

Profile* AppListServiceImpl::GetCurrentAppListProfile() {
  return profile();
}

void AppListServiceImpl::SetProfile(Profile* new_profile) {
  registrar_.RemoveAll();
  profile_ = new_profile;
  if (!profile_)
    return;

  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED,
                 content::Source<Profile>(profile_));
}

void AppListServiceImpl::InvalidatePendingProfileLoads() {
  profile_load_sequence_id_++;
}

void AppListServiceImpl::LoadProfileAsync(
    const base::FilePath& profile_file_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  InvalidatePendingProfileLoads();
  IncrementPendingProfileLoads();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager->CreateProfileAsync(
      profile_file_path,
      base::Bind(&AppListServiceImpl::OnProfileLoaded,
                 weak_factory_.GetWeakPtr(), profile_load_sequence_id_),
      string16(), string16(), false);
}

void AppListServiceImpl::OnProfileLoaded(int profile_load_sequence_id,
                                         Profile* profile,
                                         Profile::CreateStatus status) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  switch (status) {
    case Profile::CREATE_STATUS_CREATED:
      break;
    case Profile::CREATE_STATUS_INITIALIZED:
      // Only show if there has been no other profile shown since this load
      // started.
      if (profile_load_sequence_id == profile_load_sequence_id_)
        ShowAppList(profile);
      DecrementPendingProfileLoads();
      break;
    case Profile::CREATE_STATUS_FAIL:
      DecrementPendingProfileLoads();
      break;
  }
}

void AppListServiceImpl::ScheduleWarmup() {
  // Post a task to create the app list. This is posted to not impact startup
  // time.
  const int kInitWindowDelay = 5;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AppListServiceImpl::LoadProfileForInit,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kInitWindowDelay));

  // Send app list usage stats after a delay.
  const int kSendUsageStatsDelay = 5;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AppListServiceImpl::SendAppListStats),
      base::TimeDelta::FromSeconds(kSendUsageStatsDelay));
}

bool AppListServiceImpl::IsInitViewNeeded() const {
  if (!g_browser_process || g_browser_process->IsShuttingDown())
    return false;

  // We only need to initialize the view if there's no view already created and
  // there's no profile loading to be shown.
  return !HasCurrentView() && profile_load_sequence_id_ == 0;
}

void AppListServiceImpl::LoadProfileForInit() {
  if (!IsInitViewNeeded())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_file_path(
      GetAppListProfilePath(profile_manager->user_data_dir()));
  Profile* profile = profile_manager->GetProfileByPath(profile_file_path);

  if (!profile) {
    profile_manager->CreateProfileAsync(
        profile_file_path,
        base::Bind(&AppListServiceImpl::OnProfileLoadedForInit,
                   weak_factory_.GetWeakPtr(), profile_load_sequence_id_),
        string16(), string16(), false);
    return;
  }

  DoWarmupForProfile(profile);
}

void AppListServiceImpl::OnProfileLoadedForInit(int profile_load_sequence_id,
                                                Profile* profile,
                                                Profile::CreateStatus status) {
  if (!IsInitViewNeeded())
    return;

  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  DoWarmupForProfile(profile);
}

void AppListServiceImpl::IncrementPendingProfileLoads() {
  pending_profile_loads_++;
  if (pending_profile_loads_ == 1)
    chrome::StartKeepAlive();
}

void AppListServiceImpl::DecrementPendingProfileLoads() {
  pending_profile_loads_--;
  if (pending_profile_loads_ == 0)
    chrome::EndKeepAlive();
}

void AppListServiceImpl::SaveProfilePathToLocalState(
    const base::FilePath& profile_file_path) {
  g_browser_process->local_state()->SetString(
      prefs::kAppListProfile,
      profile_file_path.BaseName().MaybeAsASCII());
}
