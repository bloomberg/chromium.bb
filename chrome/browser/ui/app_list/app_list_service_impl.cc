// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_impl.h"

#include <string>

#include "apps/pref_names.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/keep_alive_service.h"
#include "chrome/browser/ui/app_list/keep_alive_service_impl.h"
#include "chrome/browser/ui/app_list/profile_loader.h"
#include "chrome/browser/ui/app_list/profile_store.h"
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

class ProfileStoreImpl : public ProfileStore {
 public:
  explicit ProfileStoreImpl(ProfileManager* profile_manager)
      : profile_manager_(profile_manager),
        weak_factory_(this) {
  }

  virtual void AddProfileObserver(ProfileInfoCacheObserver* observer) OVERRIDE {
    profile_manager_->GetProfileInfoCache().AddObserver(observer);
  }

  virtual void LoadProfileAsync(
      const base::FilePath& path,
      base::Callback<void(Profile*)> callback) OVERRIDE {
    profile_manager_->CreateProfileAsync(
        path,
        base::Bind(&ProfileStoreImpl::OnProfileCreated,
                   weak_factory_.GetWeakPtr(),
                   callback),
        base::string16(),
        base::string16(),
        std::string());
  }

  void OnProfileCreated(base::Callback<void(Profile*)> callback,
                        Profile* profile,
                        Profile::CreateStatus status) {
    switch (status) {
      case Profile::CREATE_STATUS_CREATED:
        break;
      case Profile::CREATE_STATUS_INITIALIZED:
        callback.Run(profile);
        break;
      case Profile::CREATE_STATUS_LOCAL_FAIL:
      case Profile::CREATE_STATUS_REMOTE_FAIL:
      case Profile::CREATE_STATUS_CANCELED:
        break;
      case Profile::MAX_CREATE_STATUS:
        NOTREACHED();
        break;
    }
  }

  virtual Profile* GetProfileByPath(const base::FilePath& path) OVERRIDE {
    return profile_manager_->GetProfileByPath(path);
  }

  virtual base::FilePath GetUserDataDir() OVERRIDE {
    return profile_manager_->user_data_dir();
  }

  virtual bool IsProfileManaged(const base::FilePath& profile_path) OVERRIDE {
    ProfileInfoCache& profile_info =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    size_t profile_index = profile_info.GetIndexOfProfileWithPath(profile_path);
    return profile_info.ProfileIsManagedAtIndex(profile_index);
  }

 private:
  ProfileManager* profile_manager_;
  base::WeakPtrFactory<ProfileStoreImpl> weak_factory_;
};

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
      profile_store_(new ProfileStoreImpl(
          g_browser_process->profile_manager())),
      weak_factory_(this),
      command_line_(*CommandLine::ForCurrentProcess()),
      local_state_(g_browser_process->local_state()),
      profile_loader_(new ProfileLoader(
          profile_store_.get(),
          scoped_ptr<KeepAliveService>(new KeepAliveServiceImpl))) {
  profile_store_->AddProfileObserver(this);
}

AppListServiceImpl::AppListServiceImpl(
    const CommandLine& command_line,
    PrefService* local_state,
    scoped_ptr<ProfileStore> profile_store,
    scoped_ptr<KeepAliveService> keep_alive_service)
    : profile_(NULL),
      profile_store_(profile_store.Pass()),
      weak_factory_(this),
      command_line_(command_line),
      local_state_(local_state),
      profile_loader_(new ProfileLoader(
          profile_store_.get(), keep_alive_service.Pass())) {
  profile_store_->AddProfileObserver(this);
}

AppListServiceImpl::~AppListServiceImpl() {}

void AppListServiceImpl::SetAppListNextPaintCallback(
    const base::Closure& callback) {}

void AppListServiceImpl::HandleFirstRun() {}

void AppListServiceImpl::Init(Profile* initial_profile) {}

base::FilePath AppListServiceImpl::GetProfilePath(
    const base::FilePath& user_data_dir) {
  std::string app_list_profile;
  if (local_state_->HasPrefPath(prefs::kAppListProfile))
    app_list_profile = local_state_->GetString(prefs::kAppListProfile);

  // If the user has no profile preference for the app launcher, default to the
  // last browser profile used.
  if (app_list_profile.empty() &&
      local_state_->HasPrefPath(prefs::kProfileLastUsed)) {
    app_list_profile = local_state_->GetString(prefs::kProfileLastUsed);
  }

  // If there is no last used profile recorded, use the initial profile.
  if (app_list_profile.empty())
    app_list_profile = chrome::kInitialProfile;

  return user_data_dir.AppendASCII(app_list_profile);
}

void AppListServiceImpl::SetProfilePath(const base::FilePath& profile_path) {
  // Ensure we don't set the pref to a managed user's profile path.
  // TODO(calamity): Filter out managed profiles from the settings app so this
  // can't get hit, so we can remove it.
  if (profile_store_->IsProfileManaged(profile_path))
    return;

  local_state_->SetString(
      prefs::kAppListProfile,
      profile_path.BaseName().MaybeAsASCII());
}

void AppListServiceImpl::CreateShortcut() {}

// We need to watch for profile removal to keep kAppListProfile updated.
void AppListServiceImpl::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
  // If the profile the app list uses just got deleted, reset it to the last
  // used profile.
  std::string app_list_last_profile = local_state_->GetString(
      prefs::kAppListProfile);
  if (profile_path.BaseName().MaybeAsASCII() == app_list_last_profile) {
    local_state_->SetString(prefs::kAppListProfile,
        local_state_->GetString(prefs::kProfileLastUsed));
  }
}

void AppListServiceImpl::Show() {
  profile_loader_->LoadProfileInvalidatingOtherLoads(
      GetProfilePath(profile_store_->GetUserDataDir()),
      base::Bind(&AppListServiceImpl::ShowForProfile,
                 weak_factory_.GetWeakPtr()));
}

void AppListServiceImpl::EnableAppList(Profile* initial_profile) {
  SetProfilePath(initial_profile->GetPath());
  if (local_state_->GetBoolean(prefs::kAppLauncherHasBeenEnabled))
    return;

  local_state_->SetBoolean(prefs::kAppLauncherHasBeenEnabled, true);
  CreateShortcut();
}

Profile* AppListServiceImpl::GetCurrentAppListProfile() {
  return profile();
}

void AppListServiceImpl::SetProfile(Profile* new_profile) {
  profile_ = new_profile;
}

void AppListServiceImpl::InvalidatePendingProfileLoads() {
  profile_loader_->InvalidatePendingProfileLoads();
}

void AppListServiceImpl::HandleCommandLineFlags(Profile* initial_profile) {
  if (command_line_.HasSwitch(switches::kEnableAppList))
    EnableAppList(initial_profile);

  if (command_line_.HasSwitch(switches::kResetAppListInstallState))
    local_state_->SetBoolean(prefs::kAppLauncherHasBeenEnabled, false);
}

void AppListServiceImpl::SendUsageStats() {
  // Send app list usage stats after a delay.
  const int kSendUsageStatsDelay = 5;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AppListServiceImpl::SendAppListStats),
      base::TimeDelta::FromSeconds(kSendUsageStatsDelay));
}
