// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/win/app_list_service_win.h"

#include <dwmapi.h>
#include <sstream>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/ash/app_list/app_list_service_ash.h"
#include "chrome/browser/ui/views/app_list/win/activation_tracker_win.h"
#include "chrome/browser/ui/views/app_list/win/app_list_controller_delegate_win.h"
#include "chrome/browser/ui/views/app_list/win/app_list_win.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/browser_distribution.h"
#include "content/public/browser/browser_thread.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/base/win/shell.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/updating_app_registration_data.h"
#include "chrome/installer/util/util_constants.h"
#endif  // GOOGLE_CHROME_BUILD

// static
AppListService* AppListService::Get(chrome::HostDesktopType desktop_type) {
  if (desktop_type == chrome::HOST_DESKTOP_TYPE_ASH)
    return AppListServiceAsh::GetInstance();

  return AppListServiceWin::GetInstance();
}

// static
void AppListService::InitAll(Profile* initial_profile) {
  AppListServiceAsh::GetInstance()->Init(initial_profile);
  AppListServiceWin::GetInstance()->Init(initial_profile);
}

namespace {

// Migrate chrome::kAppLauncherIsEnabled pref to
// chrome::kAppLauncherHasBeenEnabled pref.
void MigrateAppLauncherEnabledPref() {
  PrefService* prefs = g_browser_process->local_state();
  if (prefs->HasPrefPath(prefs::kAppLauncherIsEnabled)) {
    prefs->SetBoolean(prefs::kAppLauncherHasBeenEnabled,
                      prefs->GetBoolean(prefs::kAppLauncherIsEnabled));
    prefs->ClearPref(prefs::kAppLauncherIsEnabled);
  }
}

int GetAppListIconIndex() {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  return dist->GetIconIndex(BrowserDistribution::SHORTCUT_APP_LAUNCHER);
}

base::string16 GetAppListIconPath() {
  base::FilePath icon_path;
  if (!PathService::Get(base::FILE_EXE, &icon_path)) {
    NOTREACHED();
    return base::string16();
  }

  std::stringstream ss;
  ss << "," << GetAppListIconIndex();
  base::string16 result = icon_path.value();
  result.append(base::UTF8ToUTF16(ss.str()));
  return result;
}

base::string16 GetAppListShortcutName() {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  return dist->GetShortcutName(BrowserDistribution::SHORTCUT_APP_LAUNCHER);
}

CommandLine GetAppListCommandLine() {
  const char* const kSwitchesToCopy[] = { switches::kUserDataDir };
  CommandLine* current = CommandLine::ForCurrentProcess();
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
     NOTREACHED();
     return CommandLine(CommandLine::NO_PROGRAM);
  }
  CommandLine command_line(chrome_exe);
  command_line.CopySwitchesFrom(*current, kSwitchesToCopy,
                                arraysize(kSwitchesToCopy));
  command_line.AppendSwitch(switches::kShowAppList);
  return command_line;
}

base::string16 GetAppModelId() {
  // The AppModelId should be the same for all profiles in a user data directory
  // but different for different user data directories, so base it on the
  // initial profile in the current user data directory.
  base::FilePath initial_profile_path;
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUserDataDir)) {
    initial_profile_path =
        command_line->GetSwitchValuePath(switches::kUserDataDir).AppendASCII(
            chrome::kInitialProfile);
  }
  return ShellIntegration::GetAppListAppModelIdForProfile(initial_profile_path);
}

#if defined(GOOGLE_CHROME_BUILD)
void SetDidRunForNDayActiveStats() {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  base::FilePath exe_path;
  if (!PathService::Get(base::DIR_EXE, &exe_path)) {
    NOTREACHED();
    return;
  }
  bool system_install =
      !InstallUtil::IsPerUserInstall(exe_path.value().c_str());
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

// The start menu shortcut is created on first run by users that are
// upgrading. The desktop and taskbar shortcuts are created the first time the
// user enables the app list. The taskbar shortcut is created in
// |user_data_dir| and will use a Windows Application Model Id of
// |app_model_id|. This runs on the FILE thread and not in the blocking IO
// thread pool as there are other tasks running (also on the FILE thread)
// which fiddle with shortcut icons
// (ShellIntegration::MigrateWin7ShortcutsOnPath). Having different threads
// fiddle with the same shortcuts could cause race issues.
void CreateAppListShortcuts(
    const base::FilePath& user_data_dir,
    const base::string16& app_model_id,
    const web_app::ShortcutLocations& creation_locations) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);

  // Shortcut paths under which to create shortcuts.
  std::vector<base::FilePath> shortcut_paths =
      web_app::internals::GetShortcutPaths(creation_locations);

  bool pin_to_taskbar = creation_locations.in_quick_launch_bar &&
                        (base::win::GetVersion() >= base::win::VERSION_WIN7);

  // Create a shortcut in the |user_data_dir| for taskbar pinning.
  if (pin_to_taskbar)
    shortcut_paths.push_back(user_data_dir);
  bool success = true;

  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return;
  }

  base::string16 app_list_shortcut_name = GetAppListShortcutName();

  base::string16 wide_switches(GetAppListCommandLine().GetArgumentsString());

  base::win::ShortcutProperties shortcut_properties;
  shortcut_properties.set_target(chrome_exe);
  shortcut_properties.set_working_dir(chrome_exe.DirName());
  shortcut_properties.set_arguments(wide_switches);
  shortcut_properties.set_description(app_list_shortcut_name);
  shortcut_properties.set_icon(chrome_exe, GetAppListIconIndex());
  shortcut_properties.set_app_id(app_model_id);

  for (size_t i = 0; i < shortcut_paths.size(); ++i) {
    base::FilePath shortcut_file =
        shortcut_paths[i].Append(app_list_shortcut_name).
            AddExtension(installer::kLnkExt);
    if (!base::PathExists(shortcut_file.DirName()) &&
        !base::CreateDirectory(shortcut_file.DirName())) {
      NOTREACHED();
      return;
    }
    success = success && base::win::CreateOrUpdateShortcutLink(
        shortcut_file, shortcut_properties,
        base::win::SHORTCUT_CREATE_ALWAYS);
  }

  if (success && pin_to_taskbar) {
    base::FilePath shortcut_to_pin =
        user_data_dir.Append(app_list_shortcut_name).
            AddExtension(installer::kLnkExt);
    success = base::win::TaskbarPinShortcutLink(
        shortcut_to_pin.value().c_str()) && success;
  }
}

// Customizes the app list |hwnd| for Windows (eg: disable aero peek, set up
// restart params).
void SetWindowAttributes(HWND hwnd) {
  if (base::win::GetVersion() > base::win::VERSION_VISTA) {
    // Disable aero peek. Without this, hovering over the taskbar popup puts
    // Windows into a mode for switching between windows in the same
    // application. The app list has just one window, so it is just distracting.
    BOOL disable_value = TRUE;
    ::DwmSetWindowAttribute(hwnd,
                            DWMWA_DISALLOW_PEEK,
                            &disable_value,
                            sizeof(disable_value));
  }

  ui::win::SetAppIdForWindow(GetAppModelId(), hwnd);
  CommandLine relaunch = GetAppListCommandLine();
  base::string16 app_name(GetAppListShortcutName());
  ui::win::SetRelaunchDetailsForWindow(
      relaunch.GetCommandLineString(), app_name, hwnd);
  ::SetWindowText(hwnd, app_name.c_str());
  base::string16 icon_path = GetAppListIconPath();
  ui::win::SetAppIconForWindow(icon_path, hwnd);
}

}  // namespace

// static
AppListServiceWin* AppListServiceWin::GetInstance() {
  return Singleton<AppListServiceWin,
                   LeakySingletonTraits<AppListServiceWin> >::get();
}

AppListServiceWin::AppListServiceWin()
    : AppListServiceViews(scoped_ptr<AppListControllerDelegate>(
          new AppListControllerDelegateWin(this))),
      enable_app_list_on_next_init_(false) {
}

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

void AppListServiceWin::HandleFirstRun() {
  PrefService* local_state = g_browser_process->local_state();
  // If the app list is already enabled during first run, then the user had
  // opted in to the app launcher before uninstalling, so we re-enable to
  // restore shortcuts to the app list.
  // Note we can't directly create the shortcuts here because the IO thread
  // hasn't been created yet.
  enable_app_list_on_next_init_ = local_state->GetBoolean(
      prefs::kAppLauncherHasBeenEnabled);
}

void AppListServiceWin::Init(Profile* initial_profile) {
  if (enable_app_list_on_next_init_) {
    enable_app_list_on_next_init_ = false;
    EnableAppList(initial_profile, ENABLE_ON_REINSTALL);
    CreateShortcut();
  }

  ScheduleWarmup();

  MigrateAppLauncherEnabledPref();
  AppListServiceViews::Init(initial_profile);
}

void AppListServiceWin::CreateShortcut() {
  // Check if the app launcher shortcuts have ever been created before.
  // Shortcuts should only be created once. If the user unpins the taskbar
  // shortcut, they can restore it by pinning the start menu or desktop
  // shortcut.
  web_app::ShortcutLocations shortcut_locations;
  shortcut_locations.on_desktop = true;
  shortcut_locations.in_quick_launch_bar = true;
  shortcut_locations.applications_menu_location =
      web_app::APP_MENU_LOCATION_SUBDIR_CHROME;
  base::FilePath user_data_dir(
      g_browser_process->profile_manager()->user_data_dir());

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&CreateAppListShortcuts,
                 user_data_dir, GetAppModelId(), shortcut_locations));
}

void AppListServiceWin::ScheduleWarmup() {
  // Post a task to create the app list. This is posted to not impact startup
  // time.
  const int kInitWindowDelay = 30;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AppListServiceWin::LoadProfileForWarmup,
                 base::Unretained(this)),
      base::TimeDelta::FromSeconds(kInitWindowDelay));
}

bool AppListServiceWin::IsWarmupNeeded() {
  if (!g_browser_process || g_browser_process->IsShuttingDown() ||
      browser_shutdown::IsTryingToQuit()) {
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
  SetWindowAttributes(shower().app_list()->GetHWND());
  activation_tracker_.reset(new ActivationTrackerWin(this));
}

void AppListServiceWin::OnViewDismissed() {
  activation_tracker_->OnViewHidden();
}

void AppListServiceWin::MoveNearCursor(app_list::AppListView* view) {
  AppListWin::MoveNearCursor(view);
}
