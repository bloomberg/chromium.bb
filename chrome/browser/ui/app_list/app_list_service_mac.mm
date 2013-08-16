// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>

#include "apps/app_launcher.h"
#include "apps/app_shim/app_shim_handler_mac.h"
#include "apps/app_shim/app_shim_mac.h"
#include "apps/pref_names.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_mac.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/mac/app_mode_common.h"
#include "content/public/browser/browser_thread.h"
#include "grit/chrome_unscaled_resources.h"
#include "grit/google_chrome_strings.h"
#import "ui/app_list/cocoa/app_list_view_controller.h"
#import "ui/app_list/cocoa/app_list_window_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace gfx {
class ImageSkia;
}

namespace {

// Version of the app list shortcut version installed.
const int kShortcutVersion = 1;

// AppListServiceMac manages global resources needed for the app list to
// operate, and controls when the app list is opened and closed.
class AppListServiceMac : public AppListServiceImpl,
                          public apps::AppShimHandler {
 public:
  virtual ~AppListServiceMac() {}

  static AppListServiceMac* GetInstance() {
    return Singleton<AppListServiceMac,
                     LeakySingletonTraits<AppListServiceMac> >::get();
  }

  void ShowWindowNearDock();

  // AppListService overrides:
  virtual void Init(Profile* initial_profile) OVERRIDE;
  virtual void CreateForProfile(Profile* requested_profile) OVERRIDE;
  virtual void ShowForProfile(Profile* requested_profile) OVERRIDE;
  virtual void DismissAppList() OVERRIDE;
  virtual bool IsAppListVisible() const OVERRIDE;
  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE;

  // AppListServiceImpl overrides:
  virtual void CreateShortcut() OVERRIDE;
  virtual void OnSigninStatusChanged() OVERRIDE;

  // AppShimHandler overrides:
  virtual void OnShimLaunch(apps::AppShimHandler::Host* host,
                            apps::AppShimLaunchType launch_type) OVERRIDE;
  virtual void OnShimClose(apps::AppShimHandler::Host* host) OVERRIDE;
  virtual void OnShimFocus(apps::AppShimHandler::Host* host,
                           apps::AppShimFocusType focus_type) OVERRIDE;
  virtual void OnShimSetHidden(apps::AppShimHandler::Host* host,
                               bool hidden) OVERRIDE;
  virtual void OnShimQuit(apps::AppShimHandler::Host* host) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<AppListServiceMac>;

  AppListServiceMac() {}

  base::scoped_nsobject<AppListWindowController> window_controller_;
  base::scoped_nsobject<NSRunningApplication> previously_active_application_;

  // App shim hosts observing when the app list is dismissed. In normal user
  // usage there should only be one. However, it can't be guaranteed, so use
  // an ObserverList rather than handling corner cases.
  ObserverList<apps::AppShimHandler::Host> observers_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceMac);
};

class AppListControllerDelegateCocoa : public AppListControllerDelegate {
 public:
  AppListControllerDelegateCocoa();
  virtual ~AppListControllerDelegateCocoa();

 private:
  // AppListControllerDelegate overrides:
  virtual void DismissView() OVERRIDE;
  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE;
  virtual bool CanPin() OVERRIDE;
  virtual bool CanDoCreateShortcutsFlow(bool is_platform_app) OVERRIDE;
  virtual void CreateNewWindow(Profile* profile, bool incognito) OVERRIDE;
  virtual void DoCreateShortcutsFlow(Profile* profile,
                                     const std::string& extension_id) OVERRIDE;
  virtual void ActivateApp(Profile* profile,
                           const extensions::Extension* extension,
                           int event_flags) OVERRIDE;
  virtual void LaunchApp(Profile* profile,
                         const extensions::Extension* extension,
                         int event_flags) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerDelegateCocoa);
};

ShellIntegration::ShortcutInfo GetAppListShortcutInfo(
    const base::FilePath& profile_path) {
  ShellIntegration::ShortcutInfo shortcut_info;
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_CANARY) {
    shortcut_info.title =
        l10n_util::GetStringUTF16(IDS_APP_LIST_SHORTCUT_NAME_CANARY);
  } else {
    shortcut_info.title = l10n_util::GetStringUTF16(IDS_APP_LIST_SHORTCUT_NAME);
  }

  shortcut_info.extension_id = app_mode::kAppListModeId;
  shortcut_info.description = shortcut_info.title;
  shortcut_info.profile_path = profile_path;

  return shortcut_info;
}

void CreateAppListShim(const base::FilePath& profile_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  WebApplicationInfo web_app_info;
  ShellIntegration::ShortcutInfo shortcut_info =
      GetAppListShortcutInfo(profile_path);

  ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_CANARY) {
#if defined(GOOGLE_CHROME_BUILD)
    shortcut_info.favicon.Add(
        *resource_bundle.GetImageSkiaNamed(IDR_APP_LIST_CANARY_16));
    shortcut_info.favicon.Add(
        *resource_bundle.GetImageSkiaNamed(IDR_APP_LIST_CANARY_32));
    shortcut_info.favicon.Add(
        *resource_bundle.GetImageSkiaNamed(IDR_APP_LIST_CANARY_128));
    shortcut_info.favicon.Add(
        *resource_bundle.GetImageSkiaNamed(IDR_APP_LIST_CANARY_256));
#else
    NOTREACHED();
#endif
  } else {
    shortcut_info.favicon.Add(
        *resource_bundle.GetImageSkiaNamed(IDR_APP_LIST_16));
    shortcut_info.favicon.Add(
        *resource_bundle.GetImageSkiaNamed(IDR_APP_LIST_32));
    shortcut_info.favicon.Add(
        *resource_bundle.GetImageSkiaNamed(IDR_APP_LIST_128));
    shortcut_info.favicon.Add(
        *resource_bundle.GetImageSkiaNamed(IDR_APP_LIST_256));
  }

  ShellIntegration::ShortcutLocations shortcut_locations;
  PrefService* local_state = g_browser_process->local_state();
  int installed_version =
      local_state->GetInteger(apps::prefs::kAppLauncherShortcutVersion);

  // If this is a first-time install, add a dock icon. Otherwise just update
  // the target, and wait for OSX to refresh its icon caches. This might not
  // occur until a reboot, but OSX does not offer a nicer way. Deleting cache
  // files on disk and killing processes can easily result in icon corruption.
  if (installed_version == 0)
    shortcut_locations.in_quick_launch_bar = true;

  web_app::CreateShortcuts(shortcut_info,
                           shortcut_locations,
                           web_app::SHORTCUT_CREATION_AUTOMATED);

  local_state->SetInteger(apps::prefs::kAppLauncherShortcutVersion,
                          kShortcutVersion);
}

// Check that there is an app list shim. If enabling and there is not, make one.
// If the flag is not present, and there is a shim, delete it.
void CheckAppListShimOnFileThread(const base::FilePath& profile_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  const bool enable =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableAppListShim);
  base::FilePath install_path = web_app::GetAppInstallPath(
      GetAppListShortcutInfo(profile_path));
  if (enable == base::PathExists(install_path))
    return;

  if (enable) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&CreateAppListShim, profile_path));
    return;
  }

  // Sanity check because deleting things recursively is scary.
  CHECK(install_path.MatchesExtension(".app"));
  base::DeleteFile(install_path, true /* recursive */);
}

void CreateShortcutsInDefaultLocation(
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  web_app::CreateShortcuts(shortcut_info,
                           ShellIntegration::ShortcutLocations(),
                           web_app::SHORTCUT_CREATION_BY_USER);
}

NSRunningApplication* ActiveApplicationNotChrome() {
  NSArray* applications = [[NSWorkspace sharedWorkspace] runningApplications];
  for (NSRunningApplication* application in applications) {
    if (![application isActive])
      continue;

    if ([application isEqual:[NSRunningApplication currentApplication]])
      return nil;  // Chrome is active.

    return application;
  }

  return nil;
}

AppListControllerDelegateCocoa::AppListControllerDelegateCocoa() {}

AppListControllerDelegateCocoa::~AppListControllerDelegateCocoa() {}

void AppListControllerDelegateCocoa::DismissView() {
  AppListServiceMac::GetInstance()->DismissAppList();
}

gfx::NativeWindow AppListControllerDelegateCocoa::GetAppListWindow() {
  return AppListServiceMac::GetInstance()->GetAppListWindow();
}

bool AppListControllerDelegateCocoa::CanPin() {
  return false;
}

bool AppListControllerDelegateCocoa::CanDoCreateShortcutsFlow(
    bool is_platform_app) {
  return false;
}

void AppListControllerDelegateCocoa::DoCreateShortcutsFlow(
    Profile* profile, const std::string& extension_id) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(service);
  const extensions::Extension* extension =
      service->GetInstalledExtension(extension_id);
  DCHECK(extension);

  web_app::UpdateShortcutInfoAndIconForApp(
      *extension, profile, base::Bind(&CreateShortcutsInDefaultLocation));
}

void AppListControllerDelegateCocoa::CreateNewWindow(
    Profile* profile, bool incognito) {
  Profile* window_profile = incognito ?
      profile->GetOffTheRecordProfile() : profile;
  chrome::NewEmptyWindow(window_profile, chrome::GetActiveDesktop());
}

void AppListControllerDelegateCocoa::ActivateApp(
    Profile* profile, const extensions::Extension* extension, int event_flags) {
  LaunchApp(profile, extension, event_flags);
}

void AppListControllerDelegateCocoa::LaunchApp(
    Profile* profile, const extensions::Extension* extension, int event_flags) {
  chrome::OpenApplication(chrome::AppLaunchParams(
      profile, extension, NEW_FOREGROUND_TAB));
}

void AppListServiceMac::Init(Profile* initial_profile) {
  // On Mac, Init() is called multiple times for a process: any time there is no
  // browser window open and a new window is opened, and during process startup
  // to handle the silent launch case (e.g. for app shims). In the startup case,
  // a profile has not yet been determined so |initial_profile| will be NULL.
  static bool init_called_with_profile = false;
  if (initial_profile && !init_called_with_profile) {
    init_called_with_profile = true;
    HandleCommandLineFlags(initial_profile);
    PrefService* local_state = g_browser_process->local_state();
    if (!apps::IsAppLauncherEnabled()) {
      local_state->SetInteger(apps::prefs::kAppLauncherShortcutVersion, 0);

      // Not yet enabled via the Web Store. Check for the chrome://flag.
      content::BrowserThread::PostTask(
          content::BrowserThread::FILE, FROM_HERE,
          base::Bind(&CheckAppListShimOnFileThread,
                     initial_profile->GetPath()));
    } else {
      int installed_shortcut_version =
          local_state->GetInteger(apps::prefs::kAppLauncherShortcutVersion);

      if (kShortcutVersion > installed_shortcut_version)
        CreateShortcut();
    }
  }

  static bool init_called = false;
  if (init_called)
    return;

  init_called = true;
  apps::AppShimHandler::RegisterHandler(app_mode::kAppListModeId,
                                        AppListServiceMac::GetInstance());
}

void AppListServiceMac::CreateForProfile(Profile* requested_profile) {
  if (profile() == requested_profile)
    return;

  // The Objective C objects might be released at some unknown point in the
  // future, so explicitly clear references to C++ objects.
  [[window_controller_ appListViewController]
      setDelegate:scoped_ptr<app_list::AppListViewDelegate>()];

  SetProfile(requested_profile);
  scoped_ptr<app_list::AppListViewDelegate> delegate(
      new AppListViewDelegate(new AppListControllerDelegateCocoa(), profile()));
  window_controller_.reset([[AppListWindowController alloc] init]);
  [[window_controller_ appListViewController] setDelegate:delegate.Pass()];
}

void AppListServiceMac::ShowForProfile(Profile* requested_profile) {
  if (requested_profile->IsManaged())
    return;

  InvalidatePendingProfileLoads();

  if (IsAppListVisible() && (requested_profile == profile())) {
    ShowWindowNearDock();
    return;
  }

  SetProfilePath(requested_profile->GetPath());

  DismissAppList();
  CreateForProfile(requested_profile);
  ShowWindowNearDock();
}

void AppListServiceMac::DismissAppList() {
  if (!IsAppListVisible())
    return;

  // If the app list is currently the main window, it will activate the next
  // Chrome window when dismissed. But if a different application was active
  // when the app list was shown, activate that instead.
  base::scoped_nsobject<NSRunningApplication> prior_app;
  if ([[window_controller_ window] isMainWindow])
    prior_app.swap(previously_active_application_);
  else
    previously_active_application_.reset();

  // If activation is successful, the app list will lose main status and try to
  // close itself again. It can't be closed in this runloop iteration without
  // OSX deciding to raise the next Chrome window, and _then_ activating the
  // application on top. This also occurs if no activation option is given.
  if ([prior_app activateWithOptions:NSApplicationActivateIgnoringOtherApps])
    return;

  [[window_controller_ window] close];

  FOR_EACH_OBSERVER(apps::AppShimHandler::Host,
                    observers_,
                    OnAppClosed());
}

bool AppListServiceMac::IsAppListVisible() const {
  return [[window_controller_ window] isVisible];
}

void AppListServiceMac::CreateShortcut() {
  CreateAppListShim(GetProfilePath(
      g_browser_process->profile_manager()->user_data_dir()));
}

NSWindow* AppListServiceMac::GetAppListWindow() {
  return [window_controller_ window];
}

void AppListServiceMac::OnSigninStatusChanged() {
  [[window_controller_ appListViewController] onSigninStatusChanged];
}

void AppListServiceMac::OnShimLaunch(apps::AppShimHandler::Host* host,
                                     apps::AppShimLaunchType launch_type) {
  Show();
  observers_.AddObserver(host);
  host->OnAppLaunchComplete(apps::APP_SHIM_LAUNCH_SUCCESS);
}

void AppListServiceMac::OnShimClose(apps::AppShimHandler::Host* host) {
  observers_.RemoveObserver(host);
  DismissAppList();
}

void AppListServiceMac::OnShimFocus(apps::AppShimHandler::Host* host,
                                    apps::AppShimFocusType focus_type) {
  DismissAppList();
}

void AppListServiceMac::OnShimSetHidden(apps::AppShimHandler::Host* host,
                                        bool hidden) {}

void AppListServiceMac::OnShimQuit(apps::AppShimHandler::Host* host) {
  DismissAppList();
}

enum DockLocation {
  DockLocationOtherDisplay,
  DockLocationBottom,
  DockLocationLeft,
  DockLocationRight,
};

DockLocation DockLocationInDisplay(const gfx::Display& display) {
  // Assume the dock occupies part of the work area either on the left, right or
  // bottom of the display. Note in the autohide case, it is always 4 pixels.
  const gfx::Rect work_area = display.work_area();
  const gfx::Rect display_bounds = display.bounds();
  if (work_area.bottom() != display_bounds.bottom())
    return DockLocationBottom;

  if (work_area.x() != display_bounds.x())
    return DockLocationLeft;

  if (work_area.right() != display_bounds.right())
    return DockLocationRight;

  return DockLocationOtherDisplay;
}

// If |work_area_edge| is too close to the |screen_edge| (e.g. autohide dock),
// adjust |anchor| away from the edge by a constant amount to reduce overlap and
// ensure the dock icon can still be clicked to dismiss the app list.
int AdjustPointForDynamicDock(int anchor, int screen_edge, int work_area_edge) {
  const int kAutohideDockThreshold = 10;
  const int kExtraDistance = 50;  // A dock with 40 items is about this size.
  if (abs(work_area_edge - screen_edge) > kAutohideDockThreshold)
    return anchor;

  return anchor +
      (screen_edge < work_area_edge ? kExtraDistance : -kExtraDistance);
}

NSPoint GetAppListWindowOrigin(NSWindow* window) {
  gfx::Screen* const screen = gfx::Screen::GetScreenFor([window contentView]);
  // Ensure y coordinates are flipped back into AppKit's coordinate system.
  const CGFloat max_y = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]);
  if (!CGCursorIsVisible()) {
    // If Chrome is the active application, display on the same display as
    // Chrome's keyWindow since this will catch activations triggered, e.g, via
    // WebStore install. If another application is active, OSX doesn't provide a
    // reliable way to get the display in use. Fall back to the primary display
    // since it has the menu bar and is likely to be correct, e.g., for
    // activations from Spotlight.
    const gfx::NativeView key_view = [[NSApp keyWindow] contentView];
    const gfx::Rect work_area = key_view && [NSApp isActive] ?
        screen->GetDisplayNearestWindow(key_view).work_area() :
        screen->GetPrimaryDisplay().work_area();
    return NSMakePoint(work_area.x(), max_y - work_area.bottom());
  }

  gfx::Point anchor = screen->GetCursorScreenPoint();
  const gfx::Display display = screen->GetDisplayNearestPoint(anchor);
  const DockLocation dock_location = DockLocationInDisplay(display);
  const gfx::Rect display_bounds = display.bounds();

  if (dock_location == DockLocationOtherDisplay) {
    // Just display at the bottom-left of the display the cursor is on.
    return NSMakePoint(display_bounds.x(), max_y - display_bounds.bottom());
  }

  // Anchor the center of the window in a region that prevents the window
  // showing outside of the work area.
  const NSSize window_size = [window frame].size;
  const gfx::Rect work_area = display.work_area();
  gfx::Rect anchor_area = work_area;
  anchor_area.Inset(window_size.width / 2, window_size.height / 2);
  anchor.SetToMax(anchor_area.origin());
  anchor.SetToMin(anchor_area.bottom_right());

  // Move anchor to the dock, keeping the other axis aligned with the cursor.
  switch (dock_location) {
    case DockLocationBottom:
      anchor.set_y(AdjustPointForDynamicDock(
          anchor_area.bottom(), display_bounds.bottom(), work_area.bottom()));
      break;
    case DockLocationLeft:
      anchor.set_x(AdjustPointForDynamicDock(
          anchor_area.x(), display_bounds.x(), work_area.x()));
      break;
    case DockLocationRight:
      anchor.set_x(AdjustPointForDynamicDock(
          anchor_area.right(), display_bounds.right(), work_area.right()));
      break;
    default:
      NOTREACHED();
  }

  return NSMakePoint(
      anchor.x() - window_size.width / 2,
      max_y - anchor.y() - window_size.height / 2);
}

void AppListServiceMac::ShowWindowNearDock() {
  NSWindow* window = GetAppListWindow();
  DCHECK(window);
  [window setFrameOrigin:GetAppListWindowOrigin(window)];

  // Before activating, see if an application other than Chrome is currently the
  // active application, so that it can be reactivated when dismissing.
  previously_active_application_.reset([ActiveApplicationNotChrome() retain]);

  [window makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];
}

}  // namespace

// static
AppListService* AppListService::Get() {
  return AppListServiceMac::GetInstance();
}

// static
void AppListService::InitAll(Profile* initial_profile) {
  Get()->Init(initial_profile);
}
