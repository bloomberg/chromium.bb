// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/win/app_list_service_win.h"

#include <stddef.h>
#include <stdint.h>
#include <sstream>

#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/views/app_list/win/activation_tracker_win.h"
#include "chrome/browser/ui/views/app_list/win/app_list_controller_delegate_win.h"
#include "chrome/browser/ui/views/app_list/win/app_list_win.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/browser_distribution.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/base/ui_base_switches.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/updating_app_registration_data.h"
#include "chrome/installer/util/util_constants.h"
#endif  // GOOGLE_CHROME_BUILD

// static
AppListService* AppListService::Get() {
  return AppListServiceWin::GetInstance();
}

// static
void AppListService::InitAll(Profile* initial_profile,
                             const base::FilePath& profile_path) {
  AppListServiceWin::GetInstance()->Init(initial_profile);
}

namespace {

const int kUnusedAppListNoWarmupDays = 28;

#if defined(GOOGLE_CHROME_BUILD)
void SetDidRunForNDayActiveStats() {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  base::FilePath exe_path;
  if (!PathService::Get(base::DIR_EXE, &exe_path)) {
    NOTREACHED();
    return;
  }
  bool system_install = !InstallUtil::IsPerUserInstall(exe_path);
  // Using Chrome Binary dist: Chrome dist may not exist for the legacy
  // App Launcher, and App Launcher dist may be "shadow", which does not
  // contain the information needed to determine multi-install.
  // Edge case involving Canary: crbug/239163.
  BrowserDistribution* chrome_binaries_dist =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BINARIES);
  if (chrome_binaries_dist &&
      InstallUtil::IsMultiInstall(chrome_binaries_dist, system_install)) {
    UpdatingAppRegistrationData app_launcher_reg_data(
        installer::kAppLauncherGuid);
    GoogleUpdateSettings::UpdateDidRunStateForApp(
        app_launcher_reg_data, true /* did_run */);
  }
}
#endif  // GOOGLE_CHROME_BUILD

}  // namespace

// static
AppListServiceWin* AppListServiceWin::GetInstance() {
  return base::Singleton<AppListServiceWin,
                         base::LeakySingletonTraits<AppListServiceWin>>::get();
}

AppListServiceWin::AppListServiceWin()
    : AppListServiceViews(std::unique_ptr<AppListControllerDelegate>(
          new AppListControllerDelegateWin(this))) {}

AppListServiceWin::~AppListServiceWin() {
}

void AppListServiceWin::ShowForProfile(Profile* requested_profile) {
  AppListServiceViews::ShowForProfile(requested_profile);

#if defined(GOOGLE_CHROME_BUILD)
  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE, base::Bind(SetDidRunForNDayActiveStats));
#endif  // GOOGLE_CHROME_BUILD
}

void AppListServiceWin::OnLoadProfileForWarmup(Profile* initial_profile) {
  // App list profiles should not be off-the-record.
  DCHECK(!initial_profile->IsOffTheRecord());
  DCHECK(!initial_profile->IsGuestSession());

  if (!IsWarmupNeeded())
    return;

  base::Time before_warmup(base::Time::Now());
  shower().WarmupForProfile(initial_profile);
  UMA_HISTOGRAM_TIMES("Apps.AppListWarmupDuration",
                      base::Time::Now() - before_warmup);
}

void AppListServiceWin::SetAppListNextPaintCallback(void (*callback)()) {
  if (shower().app_list())
    shower().app_list()->SetNextPaintCallback(base::Bind(callback));
  else
    next_paint_callback_ = base::Bind(callback);
}

void AppListServiceWin::Init(Profile* initial_profile) {
  ScheduleWarmup();
  AppListServiceViews::Init(initial_profile);
}

void AppListServiceWin::CreateShortcut() {
  NOTREACHED();
}

void AppListServiceWin::ScheduleWarmup() {
  // Post a task to create the app list. This is posted to not impact startup
  // time.
  /* const */ int kInitWindowDelay = 30;

  // TODO(vadimt): Make kInitWindowDelay const and remove the below switch once
  // crbug.com/431326 is fixed.

  // Profiler UMA data is reported only for first 30 sec after browser startup.
  // To make all invocations of AppListServiceWin::LoadProfileForWarmup visible
  // to the server-side analysis tool, reducing this period to 10 sec in Dev
  // builds and Canary, where profiler instrumentations are enabled.
  switch (chrome::GetChannel()) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
      kInitWindowDelay = 10;
      break;

    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
    case version_info::Channel::STABLE:
      // Except on Canary, don't bother scheduling an app launcher warmup when
      // it's not already enabled. Always schedule on Canary while collecting
      // profiler data (see comment above).
      if (!IsAppLauncherEnabled())
        return;

      // Profiler instrumentations are not enabled.
      break;
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&AppListServiceWin::LoadProfileForWarmup,
                            base::Unretained(this)),
      base::TimeDelta::FromSeconds(kInitWindowDelay));
}

bool AppListServiceWin::IsWarmupNeeded() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!g_browser_process || g_browser_process->IsShuttingDown() ||
      browser_shutdown::IsTryingToQuit() ||
      command_line->HasSwitch(switches::kTestType)) {
    return false;
  }

  // Don't warm up the app list if it hasn't been used for a while. If the last
  // launch is unknown, record it as "used" on the first warmup.
  PrefService* local_state = g_browser_process->local_state();
  int64_t last_launch_time_pref =
      local_state->GetInt64(prefs::kAppListLastLaunchTime);
  if (last_launch_time_pref == 0)
    RecordAppListLastLaunch();

  base::Time last_launch_time =
      base::Time::FromInternalValue(last_launch_time_pref);
  if (base::Time::Now() - last_launch_time >
      base::TimeDelta::FromDays(kUnusedAppListNoWarmupDays)) {
    return false;
  }

  // We only need to initialize the view if there's no view already created and
  // there's no profile loading to be shown.
  return !shower().HasView() && !profile_loader().IsAnyProfileLoading();
}

void AppListServiceWin::LoadProfileForWarmup() {
  if (!IsWarmupNeeded())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path(GetProfilePath(profile_manager->user_data_dir()));

  profile_loader().LoadProfileInvalidatingOtherLoads(
      profile_path,
      base::Bind(&AppListServiceWin::OnLoadProfileForWarmup,
                 base::Unretained(this)));
}

void AppListServiceWin::OnViewBeingDestroyed() {
  activation_tracker_.reset();
  AppListServiceViews::OnViewBeingDestroyed();
}

void AppListServiceWin::OnViewCreated() {
  if (!next_paint_callback_.is_null()) {
    shower().app_list()->SetNextPaintCallback(next_paint_callback_);
    next_paint_callback_.Reset();
  }
  activation_tracker_.reset(new ActivationTrackerWin(this));
}

void AppListServiceWin::OnViewDismissed() {
  activation_tracker_->OnViewHidden();
}

void AppListServiceWin::MoveNearCursor(app_list::AppListView* view) {
  AppListWin::MoveNearCursor(view);
}
