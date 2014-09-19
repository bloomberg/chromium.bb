// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_impl.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/app_list/profile_loader.h"
#include "chrome/browser/ui/app_list/profile_store.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

namespace {

const int kDiscoverabilityTimeoutMinutes = 60;

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
  if (!g_browser_process)
    return;  // In a unit test.

  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return;  // In a unit test.

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

  virtual bool IsProfileSupervised(
      const base::FilePath& profile_path) OVERRIDE {
    ProfileInfoCache& profile_info =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    size_t profile_index = profile_info.GetIndexOfProfileWithPath(profile_path);
    return profile_index != std::string::npos &&
        profile_info.ProfileIsSupervisedAtIndex(profile_index);
  }

 private:
  ProfileManager* profile_manager_;
  base::WeakPtrFactory<ProfileStoreImpl> weak_factory_;
};

void RecordAppListDiscoverability(PrefService* local_state,
                                  bool is_startup_check) {
  // Since this task may be delayed, ensure it does not interfere with shutdown
  // when they unluckily coincide.
  if (browser_shutdown::IsTryingToQuit())
    return;

  int64 enable_time_value = local_state->GetInt64(prefs::kAppListEnableTime);
  if (enable_time_value == 0)
    return;  // Already recorded or never enabled.

  base::Time app_list_enable_time =
      base::Time::FromInternalValue(enable_time_value);
  if (is_startup_check) {
    // When checking at startup, only clear and record the "timeout" case,
    // otherwise wait for a timeout.
    base::TimeDelta time_remaining =
        app_list_enable_time +
        base::TimeDelta::FromMinutes(kDiscoverabilityTimeoutMinutes) -
        base::Time::Now();
    if (time_remaining > base::TimeDelta()) {
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&RecordAppListDiscoverability,
                     base::Unretained(local_state),
                     false),
          time_remaining);
      return;
    }
  }

  local_state->SetInt64(prefs::kAppListEnableTime, 0);

  AppListService::AppListEnableSource enable_source =
      static_cast<AppListService::AppListEnableSource>(
          local_state->GetInteger(prefs::kAppListEnableMethod));
  if (enable_source == AppListService::ENABLE_FOR_APP_INSTALL) {
    base::TimeDelta time_taken = base::Time::Now() - app_list_enable_time;
    // This means the user "discovered" the app launcher naturally, after it was
    // enabled on the first app install. Record how long it took to discover.
    // Note that the last bucket is essentially "not discovered": subtract 1
    // minute to account for clock inaccuracy.
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Apps.AppListTimeToDiscover",
        time_taken,
        base::TimeDelta::FromSeconds(1),
        base::TimeDelta::FromMinutes(kDiscoverabilityTimeoutMinutes - 1),
        10 /* bucket_count */);
  }
  UMA_HISTOGRAM_ENUMERATION("Apps.AppListHowEnabled",
                            enable_source,
                            AppListService::ENABLE_NUM_ENABLE_SOURCES);
}

}  // namespace

void AppListServiceImpl::RecordAppListLaunch() {
  RecordDailyEventFrequency(prefs::kLastAppListLaunchPing,
                            prefs::kAppListLaunchCount,
                            &SendAppListLaunch);
  RecordAppListDiscoverability(local_state_, false);
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
    : profile_store_(
          new ProfileStoreImpl(g_browser_process->profile_manager())),
      command_line_(*CommandLine::ForCurrentProcess()),
      local_state_(g_browser_process->local_state()),
      profile_loader_(new ProfileLoader(profile_store_.get())),
      weak_factory_(this) {
  profile_store_->AddProfileObserver(this);
}

AppListServiceImpl::AppListServiceImpl(const CommandLine& command_line,
                                       PrefService* local_state,
                                       scoped_ptr<ProfileStore> profile_store)
    : profile_store_(profile_store.Pass()),
      command_line_(command_line),
      local_state_(local_state),
      profile_loader_(new ProfileLoader(profile_store_.get())),
      weak_factory_(this) {
  profile_store_->AddProfileObserver(this);
}

AppListServiceImpl::~AppListServiceImpl() {}

AppListViewDelegate* AppListServiceImpl::GetViewDelegate(Profile* profile) {
  if (!view_delegate_)
    view_delegate_.reset(new AppListViewDelegate(GetControllerDelegate()));
  view_delegate_->SetProfile(profile);
  return view_delegate_.get();
}

void AppListServiceImpl::SetAppListNextPaintCallback(void (*callback)()) {}

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
  local_state_->SetString(
      prefs::kAppListProfile,
      profile_path.BaseName().MaybeAsASCII());
}

void AppListServiceImpl::CreateShortcut() {}

void AppListServiceImpl::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
  // We need to watch for profile removal to keep kAppListProfile updated, for
  // the case that the deleted profile is being used by the app list.
  std::string app_list_last_profile = local_state_->GetString(
      prefs::kAppListProfile);
  if (profile_path.BaseName().MaybeAsASCII() != app_list_last_profile)
    return;

  // Switch the app list over to a valid profile.
  // Before ProfileInfoCache::DeleteProfileFromCache() calls this function,
  // ProfileManager::ScheduleProfileForDeletion() will have checked to see if
  // the deleted profile was also "last used", and updated that setting with
  // something valid.
  local_state_->SetString(prefs::kAppListProfile,
                          local_state_->GetString(prefs::kProfileLastUsed));

  // If the app list was never shown, there won't be a |view_delegate_| yet.
  if (!view_delegate_)
    return;

  // The Chrome AppListViewDelegate now needs its profile cleared, because:
  //  1. it has many references to the profile and can't be profile-keyed, and
  //  2. the last used profile might not be loaded yet.
  //    - this loading is sometimes done by the ProfileManager asynchronously,
  //      so the app list can't just switch to that.
  // Only Mac supports showing the app list with a NULL profile, so tear down
  // the view.
  DestroyAppList();
  view_delegate_->SetProfile(NULL);
}

void AppListServiceImpl::Show() {
  profile_loader_->LoadProfileInvalidatingOtherLoads(
      GetProfilePath(profile_store_->GetUserDataDir()),
      base::Bind(&AppListServiceImpl::ShowForProfile,
                 weak_factory_.GetWeakPtr()));
}

void AppListServiceImpl::AutoShowForProfile(Profile* requested_profile) {
  if (local_state_->GetInt64(prefs::kAppListEnableTime) != 0) {
    // User has not yet discovered the app launcher. Update the enable method to
    // indicate this. It will then be recorded in UMA.
    local_state_->SetInteger(prefs::kAppListEnableMethod,
                             ENABLE_SHOWN_UNDISCOVERED);
  }
  ShowForProfile(requested_profile);
}

void AppListServiceImpl::EnableAppList(Profile* initial_profile,
                                       AppListEnableSource enable_source) {
  SetProfilePath(initial_profile->GetPath());
  // Always allow the webstore "enable" button to re-run the install flow.
  if (enable_source != AppListService::ENABLE_VIA_WEBSTORE_LINK &&
      local_state_->GetBoolean(prefs::kAppLauncherHasBeenEnabled)) {
    return;
  }

  local_state_->SetBoolean(prefs::kAppLauncherHasBeenEnabled, true);
  CreateShortcut();

  // UMA for launcher discoverability.
  local_state_->SetInt64(prefs::kAppListEnableTime,
                         base::Time::Now().ToInternalValue());
  local_state_->SetInteger(prefs::kAppListEnableMethod, enable_source);
  if (base::MessageLoop::current()) {
    // Ensure a value is recorded if the user "never" shows the app list. Note
    // there is no message loop in unit tests.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&RecordAppListDiscoverability,
                   base::Unretained(local_state_),
                   false),
        base::TimeDelta::FromMinutes(kDiscoverabilityTimeoutMinutes));
  }
}

void AppListServiceImpl::InvalidatePendingProfileLoads() {
  profile_loader_->InvalidatePendingProfileLoads();
}

void AppListServiceImpl::PerformStartupChecks(Profile* initial_profile) {
  // Except in rare, once-off cases, this just checks that a pref is "0" and
  // returns.
  RecordAppListDiscoverability(local_state_, true);

  if (command_line_.HasSwitch(switches::kResetAppListInstallState))
    local_state_->SetBoolean(prefs::kAppLauncherHasBeenEnabled, false);

  if (command_line_.HasSwitch(switches::kEnableAppList))
    EnableAppList(initial_profile, ENABLE_VIA_COMMAND_LINE);

  if (!base::MessageLoop::current())
    return;  // In a unit test.

  // Send app list usage stats after a delay.
  const int kSendUsageStatsDelay = 5;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AppListServiceImpl::SendAppListStats),
      base::TimeDelta::FromSeconds(kSendUsageStatsDelay));
}
