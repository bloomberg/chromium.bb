// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/win/app_list_service_win.h"

#include <dwmapi.h>
#include <sstream>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/app_list/app_list.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_factory.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "chrome/browser/ui/app_list/app_list_shower.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/app_list/keep_alive_service_impl.h"
#include "chrome/browser/ui/apps/app_metro_infobar_delegate_win.h"
#include "chrome/browser/ui/views/app_list/win/activation_tracker_win.h"
#include "chrome/browser/ui/views/app_list/win/app_list_controller_delegate_win.h"
#include "chrome/browser/ui/views/app_list/win/app_list_win.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "content/public/browser/browser_thread.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "grit/theme_resources.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/display.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/widget/widget.h"
#include "win8/util/win8_util.h"

#if defined(USE_AURA)
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#endif

#if defined(USE_ASH)
#include "chrome/browser/ui/app_list/app_list_service_ash.h"
#include "chrome/browser/ui/host_desktop.h"
#endif

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/installer/util/install_util.h"
#endif

// static
AppListService* AppListService::Get() {
#if defined(USE_ASH)
  if (chrome::GetActiveDesktop() == chrome::HOST_DESKTOP_TYPE_ASH)
    return chrome::GetAppListServiceAsh();
#endif

  return chrome::GetAppListServiceWin();
}

// static
void AppListService::InitAll(Profile* initial_profile) {
#if defined(USE_ASH)
  chrome::GetAppListServiceAsh()->Init(initial_profile);
#endif
  chrome::GetAppListServiceWin()->Init(initial_profile);
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

// Icons are added to the resources of the DLL using icon names. The icon index
// for the app list icon is named IDR_X_APP_LIST or (for official builds)
// IDR_X_APP_LIST_SXS for Chrome Canary. Creating shortcuts needs to specify a
// resource index, which are different to icon names.  They are 0 based and
// contiguous. As Google Chrome builds have extra icons the icon for Google
// Chrome builds need to be higher. Unfortunately these indexes are not in any
// generated header file.
int GetAppListIconIndex() {
  const int kAppListIconIndex = 5;
  const int kAppListIconIndexSxS = 6;
  const int kAppListIconIndexChromium = 1;
#if defined(GOOGLE_CHROME_BUILD)
  if (InstallUtil::IsChromeSxSProcess())
    return kAppListIconIndexSxS;
  return kAppListIconIndex;
#else
  return kAppListIconIndexChromium;
#endif
}

string16 GetAppListIconPath() {
  base::FilePath icon_path;
  if (!PathService::Get(base::FILE_EXE, &icon_path)) {
    NOTREACHED();
    return string16();
  }

  std::stringstream ss;
  ss << "," << GetAppListIconIndex();
  string16 result = icon_path.value();
  result.append(UTF8ToUTF16(ss.str()));
  return result;
}

string16 GetAppListShortcutName() {
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_CANARY)
    return l10n_util::GetStringUTF16(IDS_APP_LIST_SHORTCUT_NAME_CANARY);
  return l10n_util::GetStringUTF16(IDS_APP_LIST_SHORTCUT_NAME);
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

string16 GetAppModelId() {
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
    BrowserDistribution* app_launcher_dist =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_APP_HOST);
    GoogleUpdateSettings::UpdateDidRunStateForDistribution(
        app_launcher_dist,
        true /* did_run */,
        system_install);
  }
}

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
    const string16& app_model_id,
    const ShellIntegration::ShortcutLocations& creation_locations) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

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

  string16 app_list_shortcut_name = GetAppListShortcutName();

  string16 wide_switches(GetAppListCommandLine().GetArgumentsString());

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
        !file_util::CreateDirectory(shortcut_file.DirName())) {
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
  // Vista and lower do not offer pinning to the taskbar, which makes any
  // presence on the taskbar useless. So, hide the window on the taskbar
  // for these versions of Windows.
  if (base::win::GetVersion() <= base::win::VERSION_VISTA) {
    LONG_PTR ex_styles = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    ex_styles |= WS_EX_TOOLWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex_styles);
  }

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
  string16 app_name(GetAppListShortcutName());
  ui::win::SetRelaunchDetailsForWindow(
      relaunch.GetCommandLineString(), app_name, hwnd);
  ::SetWindowText(hwnd, app_name.c_str());
  string16 icon_path = GetAppListIconPath();
  ui::win::SetAppIconForWindow(icon_path, hwnd);
}

class AppListFactoryWin : public AppListFactory {
 public:
  explicit AppListFactoryWin(
      scoped_ptr<AppListControllerDelegate> delegate)
      : delegate_(delegate.Pass()) {}
  virtual ~AppListFactoryWin() {}

  virtual AppList* CreateAppList(
      Profile* profile,
      const base::Closure& on_should_dismiss) OVERRIDE {
    // The controller will be owned by the view delegate, and the delegate is
    // owned by the app list view. The app list view manages it's own lifetime.
    // TODO(koz): Make AppListViewDelegate take a scoped_ptr.
    AppListViewDelegate* view_delegate = new AppListViewDelegate(
        delegate_.get(), profile);
    app_list::AppListView* view = new app_list::AppListView(view_delegate);
    gfx::Point cursor = gfx::Screen::GetNativeScreen()->GetCursorScreenPoint();
    view->InitAsBubbleAtFixedLocation(NULL,
                                      &pagination_model_,
                                      cursor,
                                      views::BubbleBorder::FLOAT,
                                      false /* border_accepts_events */);
    SetWindowAttributes(view->GetHWND());
    return new AppListWin(view, on_should_dismiss);
  }

 private:
  // PaginationModel that is shared across all views.
  app_list::PaginationModel pagination_model_;

  scoped_ptr<AppListControllerDelegate> delegate_;
  DISALLOW_COPY_AND_ASSIGN(AppListFactoryWin);
};

}  // namespace

// static
AppListServiceWin* AppListServiceWin::GetInstance() {
  return Singleton<AppListServiceWin,
                   LeakySingletonTraits<AppListServiceWin> >::get();
}

AppListServiceWin::AppListServiceWin()
    : enable_app_list_on_next_init_(false),
      shower_(new AppListShower(
          scoped_ptr<AppListFactory>(
              new AppListFactoryWin(scoped_ptr<AppListControllerDelegate>(
                  new AppListControllerDelegateWin(this)))),
          scoped_ptr<KeepAliveService>(new KeepAliveServiceImpl))),
      weak_factory_(this) {}

AppListServiceWin::~AppListServiceWin() {
}

void AppListServiceWin::set_can_close(bool can_close) {
  shower_->set_can_close(can_close);
}

gfx::NativeWindow AppListServiceWin::GetAppListWindow() {
  return shower_->GetWindow();
}

AppListControllerDelegate* AppListServiceWin::CreateControllerDelegate() {
  return new AppListControllerDelegateWin(this);
}

app_list::AppListModel* AppListServiceWin::GetAppListModelForTesting() {
  return static_cast<AppListWin*>(shower_->app_list())->model();
}

void AppListServiceWin::ShowForProfile(Profile* requested_profile) {
  DCHECK(requested_profile);
  if (requested_profile->IsManaged())
    return;

  ScopedKeepAlive show_app_list_keepalive;

  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE, base::Bind(SetDidRunForNDayActiveStats));

  if (win8::IsSingleWindowMetroMode()) {
    // This request came from Windows 8 in desktop mode, but chrome is currently
    // running in Metro mode.
    AppMetroInfoBarDelegateWin::Create(
        requested_profile, AppMetroInfoBarDelegateWin::SHOW_APP_LIST,
        std::string());
    return;
  }

  InvalidatePendingProfileLoads();
  // TODO(koz): Investigate making SetProfile() call SetProfilePath() itself.
  SetProfilePath(requested_profile->GetPath());
  SetProfile(requested_profile);
  shower_->ShowForProfile(requested_profile);
  RecordAppListLaunch();
}

void AppListServiceWin::DismissAppList() {
  shower_->DismissAppList();
}

void AppListServiceWin::OnAppListClosing() {
  shower_->CloseAppList();
  SetProfile(NULL);
}

void AppListServiceWin::OnLoadProfileForWarmup(Profile* initial_profile) {
  if (!IsWarmupNeeded())
    return;

  base::Time before_warmup(base::Time::Now());
  shower_->WarmupForProfile(initial_profile);
  UMA_HISTOGRAM_TIMES("Apps.AppListWarmupDuration",
                      base::Time::Now() - before_warmup);
}

void AppListServiceWin::SetAppListNextPaintCallback(
    const base::Closure& callback) {
  app_list::AppListView::SetNextPaintCallback(callback);
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
  // In non-Ash metro mode, we can not show the app list for this process, so do
  // not bother performing Init tasks.
  if (win8::IsSingleWindowMetroMode())
    return;

  if (enable_app_list_on_next_init_) {
    enable_app_list_on_next_init_ = false;
    EnableAppList(initial_profile);
    CreateShortcut();
  }

  PrefService* prefs = g_browser_process->local_state();
  if (prefs->HasPrefPath(prefs::kRestartWithAppList) &&
      prefs->GetBoolean(prefs::kRestartWithAppList)) {
    prefs->SetBoolean(prefs::kRestartWithAppList, false);
    // If we are restarting in Metro mode we will lose focus straight away. We
    // need to reacquire focus when that happens.
    shower_->ShowAndReacquireFocus(initial_profile);
  }

  // Migrate from legacy app launcher if we are on a non-canary and non-chromium
  // build.
#if defined(GOOGLE_CHROME_BUILD)
  if (!InstallUtil::IsChromeSxSProcess() &&
      !chrome_launcher_support::GetAnyAppHostPath().empty()) {
    chrome_launcher_support::InstallationState state =
        chrome_launcher_support::GetAppLauncherInstallationState();
    if (state == chrome_launcher_support::NOT_INSTALLED) {
      // If app_host.exe is found but can't be located in the registry,
      // skip the migration as this is likely a developer build.
      return;
    } else if (state == chrome_launcher_support::INSTALLED_AT_SYSTEM_LEVEL) {
      chrome_launcher_support::UninstallLegacyAppLauncher(
          chrome_launcher_support::SYSTEM_LEVEL_INSTALLATION);
    } else if (state == chrome_launcher_support::INSTALLED_AT_USER_LEVEL) {
      chrome_launcher_support::UninstallLegacyAppLauncher(
          chrome_launcher_support::USER_LEVEL_INSTALLATION);
    }
    EnableAppList(initial_profile);
    CreateShortcut();
  }
#endif

  ScheduleWarmup();

  MigrateAppLauncherEnabledPref();
  HandleCommandLineFlags(initial_profile);
  SendUsageStats();
}

void AppListServiceWin::CreateForProfile(Profile* profile) {
  shower_->CreateViewForProfile(profile);
}

bool AppListServiceWin::IsAppListVisible() const {
  return shower_->IsAppListVisible();
}

void AppListServiceWin::CreateShortcut() {
  // Check if the app launcher shortcuts have ever been created before.
  // Shortcuts should only be created once. If the user unpins the taskbar
  // shortcut, they can restore it by pinning the start menu or desktop
  // shortcut.
  ShellIntegration::ShortcutLocations shortcut_locations;
  shortcut_locations.on_desktop = true;
  shortcut_locations.in_quick_launch_bar = true;
  shortcut_locations.in_applications_menu = true;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  shortcut_locations.applications_menu_subdir =
      dist->GetStartMenuShortcutSubfolder(
          BrowserDistribution::SUBFOLDER_CHROME);
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
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kInitWindowDelay));
}

bool AppListServiceWin::IsWarmupNeeded() {
  if (!g_browser_process || g_browser_process->IsShuttingDown())
    return false;

  // We only need to initialize the view if there's no view already created and
  // there's no profile loading to be shown.
  return !shower_->HasView() && !profile_loader().IsAnyProfileLoading();
}

void AppListServiceWin::LoadProfileForWarmup() {
  if (!IsWarmupNeeded())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path(GetProfilePath(profile_manager->user_data_dir()));

  profile_loader().LoadProfileInvalidatingOtherLoads(
      profile_path,
      base::Bind(&AppListServiceWin::OnLoadProfileForWarmup,
                 weak_factory_.GetWeakPtr()));
}

namespace chrome {

AppListService* GetAppListServiceWin() {
  return AppListServiceWin::GetInstance();
}

}  // namespace chrome
