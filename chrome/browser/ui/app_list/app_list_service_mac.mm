// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/app_list/app_list_service_mac.h"

#include <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>

#include "apps/app_shim/app_shim_mac.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate_impl.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_mac.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "grit/chrome_unscaled_resources.h"
#include "grit/google_chrome_strings.h"
#include "net/base/url_util.h"
#import "ui/app_list/cocoa/app_list_view_controller.h"
#import "ui/app_list/cocoa/app_list_window_controller.h"
#include "ui/app_list/search_box_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace gfx {
class ImageSkia;
}

// Controller for animations that show or hide the app list.
@interface AppListAnimationController : NSObject<NSAnimationDelegate> {
 @private
  // When closing, the window to close. Retained until the animation ends.
  base::scoped_nsobject<NSWindow> window_;
  // The animation started and owned by |self|. Reset when the animation ends.
  base::scoped_nsobject<NSViewAnimation> animation_;
}

// Returns whether |window_| is scheduled to be closed when the animation ends.
- (BOOL)isClosing;

// Animate |window| to show or close it, after cancelling any current animation.
// Translates from the current location to |targetOrigin| and fades in or out.
- (void)animateWindow:(NSWindow*)window
         targetOrigin:(NSPoint)targetOrigin
              closing:(BOOL)closing;

@end

namespace {

// Version of the app list shortcut version installed.
const int kShortcutVersion = 1;

// Duration of show and hide animations.
const NSTimeInterval kAnimationDuration = 0.2;

// Distance towards the screen edge that the app list moves from when showing.
const CGFloat kDistanceMovedOnShow = 20;

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
      local_state->GetInteger(prefs::kAppLauncherShortcutVersion);

  // If this is a first-time install, add a dock icon. Otherwise just update
  // the target, and wait for OSX to refresh its icon caches. This might not
  // occur until a reboot, but OSX does not offer a nicer way. Deleting cache
  // files on disk and killing processes can easily result in icon corruption.
  if (installed_version == 0)
    shortcut_locations.in_quick_launch_bar = true;

  web_app::CreateShortcuts(shortcut_info,
                           shortcut_locations,
                           web_app::SHORTCUT_CREATION_AUTOMATED);

  local_state->SetInteger(prefs::kAppLauncherShortcutVersion,
                          kShortcutVersion);
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

void GetAppListWindowOrigins(
    NSWindow* window, NSPoint* target_origin, NSPoint* start_origin) {
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
    *target_origin = NSMakePoint(work_area.x(), max_y - work_area.bottom());
    *start_origin = *target_origin;
    return;
  }

  gfx::Point anchor = screen->GetCursorScreenPoint();
  const gfx::Display display = screen->GetDisplayNearestPoint(anchor);
  const DockLocation dock_location = DockLocationInDisplay(display);
  const gfx::Rect display_bounds = display.bounds();

  if (dock_location == DockLocationOtherDisplay) {
    // Just display at the bottom-left of the display the cursor is on.
    *target_origin = NSMakePoint(display_bounds.x(),
                                 max_y - display_bounds.bottom());
    *start_origin = *target_origin;
    return;
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

  *target_origin = NSMakePoint(anchor.x() - window_size.width / 2,
                               max_y - anchor.y() - window_size.height / 2);
  *start_origin = *target_origin;

  switch (dock_location) {
    case DockLocationBottom:
      start_origin->y -= kDistanceMovedOnShow;
      break;
    case DockLocationLeft:
      start_origin->x -= kDistanceMovedOnShow;
      break;
    case DockLocationRight:
      start_origin->x += kDistanceMovedOnShow;
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace

AppListServiceMac::AppListServiceMac()
    : profile_(NULL) {
  animation_controller_.reset([[AppListAnimationController alloc] init]);
}

AppListServiceMac::~AppListServiceMac() {}

// static
AppListServiceMac* AppListServiceMac::GetInstance() {
  return Singleton<AppListServiceMac,
                   LeakySingletonTraits<AppListServiceMac> >::get();
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
    if (!IsAppLauncherEnabled()) {
      local_state->SetInteger(prefs::kAppLauncherShortcutVersion, 0);
    } else {
      int installed_shortcut_version =
          local_state->GetInteger(prefs::kAppLauncherShortcutVersion);

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

Profile* AppListServiceMac::GetCurrentAppListProfile() {
  return profile_;
}

void AppListServiceMac::CreateForProfile(Profile* requested_profile) {
  if (profile_ == requested_profile)
    return;

  profile_ = requested_profile;

  if (window_controller_) {
    // Clear the search box.
    [[window_controller_ appListViewController] searchBoxModel]
        ->SetText(base::string16());
  } else {
    window_controller_.reset([[AppListWindowController alloc] init]);
  }

  scoped_ptr<app_list::AppListViewDelegate> delegate(
      new AppListViewDelegate(
          scoped_ptr<AppListControllerDelegate>(
              new AppListControllerDelegateImpl(this)), profile_));
  [[window_controller_ appListViewController] setDelegate:delegate.Pass()];
}

void AppListServiceMac::ShowForProfile(Profile* requested_profile) {
  if (requested_profile->IsManaged())
    return;

  InvalidatePendingProfileLoads();

  if (requested_profile == profile_) {
    ShowWindowNearDock();
    return;
  }

  SetProfilePath(requested_profile->GetPath());
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

  [animation_controller_ animateWindow:[window_controller_ window]
                          targetOrigin:last_start_origin_
                               closing:YES];
}

bool AppListServiceMac::IsAppListVisible() const {
  return [[window_controller_ window] isVisible] &&
      ![animation_controller_ isClosing];
}

void AppListServiceMac::EnableAppList(Profile* initial_profile) {
  AppListServiceImpl::EnableAppList(initial_profile);
  AppController* controller = [NSApp delegate];
  [controller initAppShimMenuController];
}

void AppListServiceMac::CreateShortcut() {
  CreateAppListShim(GetProfilePath(
      g_browser_process->profile_manager()->user_data_dir()));
}

NSWindow* AppListServiceMac::GetAppListWindow() {
  return [window_controller_ window];
}

AppListControllerDelegate* AppListServiceMac::CreateControllerDelegate() {
  return new AppListControllerDelegateImpl(this);
}

void AppListServiceMac::OnShimLaunch(apps::AppShimHandler::Host* host,
                                     apps::AppShimLaunchType launch_type,
                                     const std::vector<base::FilePath>& files) {
  if (IsAppListVisible())
    DismissAppList();
  else
    Show();

  // Always close the shim process immediately.
  host->OnAppLaunchComplete(apps::APP_SHIM_LAUNCH_DUPLICATE_HOST);
}

void AppListServiceMac::OnShimClose(apps::AppShimHandler::Host* host) {}

void AppListServiceMac::OnShimFocus(apps::AppShimHandler::Host* host,
                                    apps::AppShimFocusType focus_type,
                                    const std::vector<base::FilePath>& files) {}

void AppListServiceMac::OnShimSetHidden(apps::AppShimHandler::Host* host,
                                        bool hidden) {}

void AppListServiceMac::OnShimQuit(apps::AppShimHandler::Host* host) {}

void AppListServiceMac::ShowWindowNearDock() {
  if (IsAppListVisible())
    return;

  NSWindow* window = GetAppListWindow();
  DCHECK(window);
  NSPoint target_origin;
  GetAppListWindowOrigins(window, &target_origin, &last_start_origin_);
  [window setFrameOrigin:last_start_origin_];

  // Before activating, see if an application other than Chrome is currently the
  // active application, so that it can be reactivated when dismissing.
  previously_active_application_.reset([ActiveApplicationNotChrome() retain]);

  [animation_controller_ animateWindow:[window_controller_ window]
                          targetOrigin:target_origin
                               closing:NO];
  [window makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];
  RecordAppListLaunch();
}

// static
AppListService* AppListService::Get(chrome::HostDesktopType desktop_type) {
  return AppListServiceMac::GetInstance();
}

// static
void AppListService::InitAll(Profile* initial_profile) {
  AppListServiceMac::GetInstance()->Init(initial_profile);
}

@implementation AppListAnimationController

- (BOOL)isClosing {
  return !!window_;
}

- (void)animateWindow:(NSWindow*)window
         targetOrigin:(NSPoint)targetOrigin
              closing:(BOOL)closing {
  // First, stop the existing animation, if there is one.
  [animation_ stopAnimation];

  NSRect targetFrame = [window frame];
  targetFrame.origin = targetOrigin;

  // NSViewAnimation has a quirk when setting the curve to NSAnimationEaseOut
  // where it attempts to auto-reverse the animation. FadeOut becomes FadeIn
  // (good), but FrameKey is also switched (bad). So |targetFrame| needs to be
  // put on the StartFrameKey when using NSAnimationEaseOut for showing.
  NSArray* animationArray = @[
    @{
      NSViewAnimationTargetKey : window,
      NSViewAnimationEffectKey : NSViewAnimationFadeOutEffect,
      (closing ? NSViewAnimationEndFrameKey : NSViewAnimationStartFrameKey) :
          [NSValue valueWithRect:targetFrame]
    }
  ];
  animation_.reset(
      [[NSViewAnimation alloc] initWithViewAnimations:animationArray]);
  [animation_ setDuration:kAnimationDuration];
  [animation_ setDelegate:self];

  if (closing) {
    [animation_ setAnimationCurve:NSAnimationEaseIn];
    window_.reset([window retain]);
  } else {
    [window setAlphaValue:0.0f];
    [animation_ setAnimationCurve:NSAnimationEaseOut];
    window_.reset();
  }
  [animation_ startAnimation];
}

- (void)animationDidEnd:(NSAnimation*)animation {
  [window_ close];
  window_.reset();
  animation_.reset();

  apps::AppShimHandler::MaybeTerminate();
}

@end
