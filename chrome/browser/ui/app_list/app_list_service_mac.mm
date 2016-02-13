// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/app_list/app_list_service_mac.h"

#include <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_positioner.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_service_cocoa_mac.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_mac.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"
#include "grit/chrome_unscaled_resources.h"
#include "net/base/url_util.h"
#include "ui/app_list/app_list_switches.h"
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

// Called on the UI thread once the animation has completed to reset the
// animation state, close the window (if it is a close animation), and possibly
// terminate Chrome.
- (void)cleanupOnUIThread;

@end

namespace {

// Version of the app list shortcut version installed.
const int kShortcutVersion = 4;

// Duration of show and hide animations.
const NSTimeInterval kAnimationDuration = 0.2;

// Distance towards the screen edge that the app list moves from when showing.
const CGFloat kDistanceMovedOnShow = 20;

scoped_ptr<web_app::ShortcutInfo> GetAppListShortcutInfo(
    const base::FilePath& profile_path) {
  scoped_ptr<web_app::ShortcutInfo> shortcut_info(new web_app::ShortcutInfo);
  version_info::Channel channel = chrome::GetChannel();
  if (channel == version_info::Channel::CANARY) {
    shortcut_info->title =
        l10n_util::GetStringUTF16(IDS_APP_LIST_SHORTCUT_NAME_CANARY);
  } else {
    shortcut_info->title =
        l10n_util::GetStringUTF16(IDS_APP_LIST_SHORTCUT_NAME);
  }

  shortcut_info->extension_id = app_mode::kAppListModeId;
  shortcut_info->description = shortcut_info->title;
  shortcut_info->profile_path = profile_path;

  return shortcut_info;
}

void CreateAppListShim(const base::FilePath& profile_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  WebApplicationInfo web_app_info;
  scoped_ptr<web_app::ShortcutInfo> shortcut_info =
      GetAppListShortcutInfo(profile_path);

  ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();
  version_info::Channel channel = chrome::GetChannel();
  if (channel == version_info::Channel::CANARY) {
#if defined(GOOGLE_CHROME_BUILD)
    shortcut_info->favicon.Add(
        *resource_bundle.GetImageSkiaNamed(IDR_APP_LIST_CANARY_16));
    shortcut_info->favicon.Add(
        *resource_bundle.GetImageSkiaNamed(IDR_APP_LIST_CANARY_32));
    shortcut_info->favicon.Add(
        *resource_bundle.GetImageSkiaNamed(IDR_APP_LIST_CANARY_128));
    shortcut_info->favicon.Add(
        *resource_bundle.GetImageSkiaNamed(IDR_APP_LIST_CANARY_256));
#else
    NOTREACHED();
#endif
  } else {
    shortcut_info->favicon.Add(
        *resource_bundle.GetImageSkiaNamed(IDR_APP_LIST_16));
    shortcut_info->favicon.Add(
        *resource_bundle.GetImageSkiaNamed(IDR_APP_LIST_32));
    shortcut_info->favicon.Add(
        *resource_bundle.GetImageSkiaNamed(IDR_APP_LIST_128));
    shortcut_info->favicon.Add(
        *resource_bundle.GetImageSkiaNamed(IDR_APP_LIST_256));
  }

  web_app::ShortcutLocations shortcut_locations;
  PrefService* local_state = g_browser_process->local_state();
  int installed_version =
      local_state->GetInteger(prefs::kAppLauncherShortcutVersion);

  // If this is a first-time install, add a dock icon. Otherwise just update
  // the target, and wait for OSX to refresh its icon caches. This might not
  // occur until a reboot, but OSX does not offer a nicer way. Deleting cache
  // files on disk and killing processes can easily result in icon corruption.
  if (installed_version == 0)
    shortcut_locations.in_quick_launch_bar = true;

  web_app::CreateNonAppShortcut(shortcut_locations, std::move(shortcut_info));

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

// Determines which screen edge the dock is aligned to.
AppListPositioner::ScreenEdge DockLocationInDisplay(
    const gfx::Display& display) {
  // Assume the dock occupies part of the work area either on the left, right or
  // bottom of the display. Note in the autohide case, it is always 4 pixels.
  const gfx::Rect work_area = display.work_area();
  const gfx::Rect display_bounds = display.bounds();
  if (work_area.bottom() != display_bounds.bottom())
    return AppListPositioner::SCREEN_EDGE_BOTTOM;

  if (work_area.x() != display_bounds.x())
    return AppListPositioner::SCREEN_EDGE_LEFT;

  if (work_area.right() != display_bounds.right())
    return AppListPositioner::SCREEN_EDGE_RIGHT;

  return AppListPositioner::SCREEN_EDGE_UNKNOWN;
}

// If |display|'s work area is too close to its boundary on |dock_edge|, adjust
// the work area away from the edge by a constant amount to reduce overlap and
// ensure the dock icon can still be clicked to dismiss the app list.
void AdjustWorkAreaForDock(const gfx::Display& display,
                           AppListPositioner* positioner,
                           AppListPositioner::ScreenEdge dock_edge) {
  const int kAutohideDockThreshold = 10;
  const int kExtraDistance = 50;  // A dock with 40 items is about this size.

  const gfx::Rect work_area = display.work_area();
  const gfx::Rect display_bounds = display.bounds();

  switch (dock_edge) {
    case AppListPositioner::SCREEN_EDGE_LEFT:
      if (work_area.x() - display_bounds.x() <= kAutohideDockThreshold)
        positioner->WorkAreaInset(kExtraDistance, 0, 0, 0);
      break;
    case AppListPositioner::SCREEN_EDGE_RIGHT:
      if (display_bounds.right() - work_area.right() <= kAutohideDockThreshold)
        positioner->WorkAreaInset(0, 0, kExtraDistance, 0);
      break;
    case AppListPositioner::SCREEN_EDGE_BOTTOM:
      if (display_bounds.bottom() - work_area.bottom() <=
          kAutohideDockThreshold) {
        positioner->WorkAreaInset(0, 0, 0, kExtraDistance);
      }
      break;
    case AppListPositioner::SCREEN_EDGE_UNKNOWN:
    case AppListPositioner::SCREEN_EDGE_TOP:
      NOTREACHED();
      break;
  }
}

void GetAppListWindowOrigins(
    NSWindow* window, NSPoint* target_origin, NSPoint* start_origin) {
  gfx::Screen* const screen = gfx::Screen::GetScreen();
  // Ensure y coordinates are flipped back into AppKit's coordinate system.
  bool cursor_is_visible = CGCursorIsVisible();
  gfx::Display display;
  gfx::Point cursor;
  if (!cursor_is_visible) {
    // If Chrome is the active application, display on the same display as
    // Chrome's keyWindow since this will catch activations triggered, e.g, via
    // WebStore install. If another application is active, OSX doesn't provide a
    // reliable way to get the display in use. Fall back to the primary display
    // since it has the menu bar and is likely to be correct, e.g., for
    // activations from Spotlight.
    const gfx::NativeView key_view = [[NSApp keyWindow] contentView];
    display = key_view && [NSApp isActive] ?
        screen->GetDisplayNearestWindow(key_view) :
        screen->GetPrimaryDisplay();
  } else {
    cursor = screen->GetCursorScreenPoint();
    display = screen->GetDisplayNearestPoint(cursor);
  }

  const NSSize ns_window_size = [window frame].size;
  gfx::Size window_size(ns_window_size.width, ns_window_size.height);
  int primary_display_height =
      NSMaxY([[[NSScreen screens] firstObject] frame]);
  AppListServiceMac::FindAnchorPoint(window_size,
                                     display,
                                     primary_display_height,
                                     cursor_is_visible,
                                     cursor,
                                     target_origin,
                                     start_origin);
}

AppListServiceMac* GetActiveInstance() {
  if (app_list::switches::IsMacViewsAppListEnabled()) {
#if defined(TOOLKIT_VIEWS)
    // TODO(tapted): Return AppListServiceViewsMac instance.
#else
    NOTREACHED();
#endif
  }
  return AppListServiceCocoaMac::GetInstance();
}

}  // namespace

AppListServiceMac::AppListServiceMac() {
  animation_controller_.reset([[AppListAnimationController alloc] init]);
}

AppListServiceMac::~AppListServiceMac() {}

// static
void AppListServiceMac::FindAnchorPoint(const gfx::Size& window_size,
                                        const gfx::Display& display,
                                        int primary_display_height,
                                        bool cursor_is_visible,
                                        const gfx::Point& cursor,
                                        NSPoint* target_origin,
                                        NSPoint* start_origin) {
  AppListPositioner positioner(display, window_size, 0);
  AppListPositioner::ScreenEdge dock_location = DockLocationInDisplay(display);

  gfx::Point anchor;
  // Snap to the dock edge. If the cursor is greater than the window
  // width/height away or not visible, anchor to the center of the dock.
  // Otherwise, anchor to the cursor position.
  if (dock_location == AppListPositioner::SCREEN_EDGE_UNKNOWN) {
    anchor = positioner.GetAnchorPointForScreenCorner(
        AppListPositioner::SCREEN_CORNER_BOTTOM_LEFT);
  } else {
    int snap_distance =
        dock_location == AppListPositioner::SCREEN_EDGE_BOTTOM ||
                dock_location == AppListPositioner::SCREEN_EDGE_TOP ?
            window_size.height() :
            window_size.width();
    // Subtract the dock area since the display's default work_area will not
    // subtract it if the dock is set to auto-hide, and the app list should
    // never overlap the dock.
    AdjustWorkAreaForDock(display, &positioner, dock_location);
    if (!cursor_is_visible || positioner.GetCursorDistanceFromShelf(
                                  dock_location, cursor) > snap_distance) {
      anchor = positioner.GetAnchorPointForShelfCenter(dock_location);
    } else {
      anchor = positioner.GetAnchorPointForShelfCursor(dock_location, cursor);
    }
  }

  *target_origin = NSMakePoint(
      anchor.x() - window_size.width() / 2,
      primary_display_height - anchor.y() - window_size.height() / 2);
  *start_origin = *target_origin;

  // If the launcher is anchored to the dock (regardless of whether the cursor
  // is visible), animate in inwards from the edge of screen
  switch (dock_location) {
    case AppListPositioner::SCREEN_EDGE_UNKNOWN:
      break;
    case AppListPositioner::SCREEN_EDGE_LEFT:
      start_origin->x -= kDistanceMovedOnShow;
      break;
    case AppListPositioner::SCREEN_EDGE_RIGHT:
      start_origin->x += kDistanceMovedOnShow;
      break;
    case AppListPositioner::SCREEN_EDGE_TOP:
      NOTREACHED();
      break;
    case AppListPositioner::SCREEN_EDGE_BOTTOM:
      start_origin->y -= kDistanceMovedOnShow;
      break;
  }
}

void AppListServiceMac::Init(Profile* initial_profile) {
  InitWithProfilePath(initial_profile, initial_profile->GetPath());
}

void AppListServiceMac::InitWithProfilePath(
    Profile* initial_profile,
    const base::FilePath& profile_path) {
  // On Mac, Init() is called multiple times for a process: any time there is no
  // browser window open and a new window is opened, and during process startup
  // to handle the silent launch case (e.g. for app shims). In the startup case,
  // a profile has not yet been determined so |initial_profile| will be NULL.
  static bool init_called_with_profile = false;
  if (initial_profile && !init_called_with_profile) {
    init_called_with_profile = true;
    PerformStartupChecks(initial_profile);
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
  apps::AppShimHandler::RegisterHandler(app_mode::kAppListModeId, this);

  // Handle the case where Chrome was not running and was started with the app
  // launcher shim. The profile has not yet been loaded. To improve response
  // times, start animating an empty window which will be populated via
  // OnShimLaunch(). Note that if --silent-launch is not also passed, the window
  // will instead populate via StartupBrowserCreator::Launch(). Shim-initiated
  // launches will always have --silent-launch.
  if (base::CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kShowAppList)) {
    // Do not show the launcher window when the profile is locked, or if it
    // can't be displayed unpopulated. In the latter case, the Show will occur
    // in OnShimLaunch() or AppListService::HandleLaunchCommandLine().
    ProfileAttributesEntry* entry = nullptr;
    bool has_entry = g_browser_process->profile_manager()->
        GetProfileAttributesStorage().
        GetProfileAttributesWithPath(profile_path, &entry);
    if (has_entry && !entry->IsSigninRequired() && ReadyToShow())
      ShowWindowNearDock();
  }
}

void AppListServiceMac::DismissAppList() {
  if (!IsAppListVisible())
    return;

  NSWindow* app_list_window = GetNativeWindow();
  // If the app list is currently the main window, it will activate the next
  // Chrome window when dismissed. But if a different application was active
  // when the app list was shown, activate that instead.
  base::scoped_nsobject<NSRunningApplication> prior_app;
  if ([app_list_window isMainWindow])
    prior_app.swap(previously_active_application_);
  else
    previously_active_application_.reset();

  // If activation is successful, the app list will lose main status and try to
  // close itself again. It can't be closed in this runloop iteration without
  // OSX deciding to raise the next Chrome window, and _then_ activating the
  // application on top. This also occurs if no activation option is given.
  if ([prior_app activateWithOptions:NSApplicationActivateIgnoringOtherApps])
    return;

  [animation_controller_ animateWindow:app_list_window
                          targetOrigin:last_start_origin_
                               closing:YES];
}

void AppListServiceMac::ShowForCustomLauncherPage(Profile* profile) {
  NOTIMPLEMENTED();
}

void AppListServiceMac::HideCustomLauncherPage() {
  NOTIMPLEMENTED();
}

bool AppListServiceMac::IsAppListVisible() const {
  return [GetNativeWindow() isVisible] &&
      ![animation_controller_ isClosing];
}

void AppListServiceMac::EnableAppList(Profile* initial_profile,
                                      AppListEnableSource enable_source) {
  AppListServiceImpl::EnableAppList(initial_profile, enable_source);
  AppController* controller = [NSApp delegate];
  [controller initAppShimMenuController];
}

void AppListServiceMac::CreateShortcut() {
  CreateAppListShim(GetProfilePath(
      g_browser_process->profile_manager()->user_data_dir()));
}

NSWindow* AppListServiceMac::GetAppListWindow() {
  return GetNativeWindow();
}

void AppListServiceMac::OnShimLaunch(apps::AppShimHandler::Host* host,
                                     apps::AppShimLaunchType launch_type,
                                     const std::vector<base::FilePath>& files) {
  if (GetCurrentAppListProfile() && IsAppListVisible()) {
    DismissAppList();
  } else {
    // Start by showing a possibly empty window to handle the case where Chrome
    // is running, but hasn't yet loaded the app launcher profile.
    if (ReadyToShow())
      ShowWindowNearDock();
    Show();
  }

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

  [animation_controller_ animateWindow:window
                          targetOrigin:target_origin
                               closing:NO];
  [window makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];
  RecordAppListLaunch();
}

void AppListServiceMac::WindowAnimationDidEnd() {
  [animation_controller_ cleanupOnUIThread];
}

// static
AppListService* AppListService::Get() {
  return GetActiveInstance();
}

// static
void AppListService::InitAll(Profile* initial_profile,
                             const base::FilePath& profile_path) {
  GetActiveInstance()->InitWithProfilePath(initial_profile, profile_path);
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
  // This once used a threaded animation, but AppKit would too often ignore
  // -[NSView canDrawConcurrently:] and just redraw whole view hierarchies on
  // the animation thread anyway, creating a minefield of race conditions.
  // Non-threaded means the animation isn't as smooth and doesn't begin unless
  // the UI runloop has spun up (after profile loading).
  [animation_ setAnimationBlockingMode:NSAnimationNonblocking];

  [animation_ startAnimation];
}

- (void)cleanupOnUIThread {
  bool closing = [self isClosing];
  [window_ close];
  window_.reset();
  animation_.reset();

  if (closing)
    apps::AppShimHandler::MaybeTerminate();
}

- (void)animationDidEnd:(NSAnimation*)animation {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&AppListServiceMac::WindowAnimationDidEnd,
                 base::Unretained(GetActiveInstance())));
}

@end
