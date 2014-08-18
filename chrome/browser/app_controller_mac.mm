// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/app_controller_mac.h"

#include "apps/app_shim/extension_app_shim_handler_mac.h"
#include "apps/app_window_registry.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/background/background_application_list_model.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/mac/mac_startup_profiler.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_mac.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#import "chrome/browser/ui/cocoa/apps/app_shim_menu_controller_mac.h"
#include "chrome/browser/ui/cocoa/apps/quit_with_apps_controller_mac.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#import "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/confirm_quit.h"
#import "chrome/browser/ui/cocoa/confirm_quit_panel_controller.h"
#import "chrome/browser/ui/cocoa/encoding_menu_controller_delegate_mac.h"
#import "chrome/browser/ui/cocoa/history_menu_bridge.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#import "chrome/browser/ui/cocoa/profiles/profile_menu_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_window_controller.h"
#include "chrome/browser/ui/cocoa/task_manager_mac.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_class_mac.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/browser/extension_system.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/filename_util.h"
#include "ui/base/cocoa/focus_window_set.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

using base::UserMetricsAction;
using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;

namespace {

// Declare notification names from the 10.7 SDK.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7
NSString* NSPopoverDidShowNotification = @"NSPopoverDidShowNotification";
NSString* NSPopoverDidCloseNotification = @"NSPopoverDidCloseNotification";
#endif

// How long we allow a workspace change notification to wait to be
// associated with a dock activation. The animation lasts 250ms. See
// applicationShouldHandleReopen:hasVisibleWindows:.
static const int kWorkspaceChangeTimeoutMs = 500;

// True while AppController is calling chrome::NewEmptyWindow(). We need a
// global flag here, analogue to StartupBrowserCreator::InProcessStartup()
// because otherwise the SessionService will try to restore sessions when we
// make a new window while there are no other active windows.
bool g_is_opening_new_window = false;

// Activates a browser window having the given profile (the last one active) if
// possible and returns a pointer to the activate |Browser| or NULL if this was
// not possible. If the last active browser is minimized (in particular, if
// there are only minimized windows), it will unminimize it.
Browser* ActivateBrowser(Profile* profile) {
  Browser* browser = chrome::FindLastActiveWithProfile(
      profile->IsGuestSession() ? profile->GetOffTheRecordProfile() : profile,
      chrome::HOST_DESKTOP_TYPE_NATIVE);
  if (browser)
    browser->window()->Activate();
  return browser;
}

// Creates an empty browser window with the given profile and returns a pointer
// to the new |Browser|.
Browser* CreateBrowser(Profile* profile) {
  {
    base::AutoReset<bool> auto_reset_in_run(&g_is_opening_new_window, true);
    chrome::NewEmptyWindow(profile, chrome::HOST_DESKTOP_TYPE_NATIVE);
  }

  Browser* browser = chrome::GetLastActiveBrowser();
  CHECK(browser);
  return browser;
}

// Activates a browser window having the given profile (the last one active) if
// possible or creates an empty one if necessary. Returns a pointer to the
// activated/new |Browser|.
Browser* ActivateOrCreateBrowser(Profile* profile) {
  if (Browser* browser = ActivateBrowser(profile))
    return browser;
  return CreateBrowser(profile);
}

CFStringRef BaseBundleID_CFString() {
  NSString* base_bundle_id =
      [NSString stringWithUTF8String:base::mac::BaseBundleID()];
  return base::mac::NSToCFCast(base_bundle_id);
}

// This callback synchronizes preferences (under "org.chromium.Chromium" or
// "com.google.Chrome"), in particular, writes them out to disk.
void PrefsSyncCallback() {
  if (!CFPreferencesAppSynchronize(BaseBundleID_CFString()))
    LOG(WARNING) << "Error recording application bundle path.";
}

// Record the location of the application bundle (containing the main framework)
// from which Chromium was loaded. This is used by app mode shims to find
// Chromium.
void RecordLastRunAppBundlePath() {
  // Going up three levels from |chrome::GetVersionedDirectory()| gives the
  // real, user-visible app bundle directory. (The alternatives give either the
  // framework's path or the initial app's path, which may be an app mode shim
  // or a unit test.)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::FilePath app_bundle_path =
      chrome::GetVersionedDirectory().DirName().DirName().DirName();
  base::ScopedCFTypeRef<CFStringRef> app_bundle_path_cfstring(
      base::SysUTF8ToCFStringRef(app_bundle_path.value()));
  CFPreferencesSetAppValue(
      base::mac::NSToCFCast(app_mode::kLastRunAppBundlePathPrefsKey),
      app_bundle_path_cfstring, BaseBundleID_CFString());

  // Sync after a delay avoid I/O contention on startup; 1500 ms is plenty.
  BrowserThread::PostDelayedTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&PrefsSyncCallback),
      base::TimeDelta::FromMilliseconds(1500));
}

bool IsProfileSignedOut(Profile* profile) {
  // The signed out status only makes sense at the moment in the context of the
  // --new-profile-management flag.
  if (!switches::IsNewProfileManagement())
    return false;
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  if (profile_index == std::string::npos)
    return false;
  return cache.ProfileIsSigninRequiredAtIndex(profile_index);
}

}  // anonymous namespace

@interface AppController (Private)
- (void)initMenuState;
- (void)initProfileMenu;
- (void)updateConfirmToQuitPrefMenuItem:(NSMenuItem*)item;
- (void)updateDisplayMessageCenterPrefMenuItem:(NSMenuItem*)item;
- (void)registerServicesMenuTypesTo:(NSApplication*)app;
- (void)openUrls:(const std::vector<GURL>&)urls;
- (void)getUrl:(NSAppleEventDescriptor*)event
     withReply:(NSAppleEventDescriptor*)reply;
- (void)windowLayeringDidChange:(NSNotification*)inNotification;
- (void)activeSpaceDidChange:(NSNotification*)inNotification;
- (void)windowChangedToProfile:(Profile*)profile;
- (void)checkForAnyKeyWindows;
- (BOOL)userWillWaitForInProgressDownloads:(int)downloadCount;
- (BOOL)shouldQuitWithInProgressDownloads;
- (void)executeApplication:(id)sender;
- (void)profileWasRemoved:(const base::FilePath&)profilePath;
@end

class AppControllerProfileObserver : public ProfileInfoCacheObserver {
 public:
  AppControllerProfileObserver(
      ProfileManager* profile_manager, AppController* app_controller)
      : profile_manager_(profile_manager),
        app_controller_(app_controller) {
    DCHECK(profile_manager_);
    DCHECK(app_controller_);
    profile_manager_->GetProfileInfoCache().AddObserver(this);
  }

  virtual ~AppControllerProfileObserver() {
    DCHECK(profile_manager_);
    profile_manager_->GetProfileInfoCache().RemoveObserver(this);
  }

 private:
  // ProfileInfoCacheObserver implementation:

  virtual void OnProfileAdded(const base::FilePath& profile_path) OVERRIDE {
  }

  virtual void OnProfileWasRemoved(
      const base::FilePath& profile_path,
      const base::string16& profile_name) OVERRIDE {
    // When a profile is deleted we need to notify the AppController,
    // so it can correctly update its pointer to the last used profile.
    [app_controller_ profileWasRemoved:profile_path];
  }

  virtual void OnProfileWillBeRemoved(
      const base::FilePath& profile_path) OVERRIDE {
  }

  virtual void OnProfileNameChanged(
      const base::FilePath& profile_path,
      const base::string16& old_profile_name) OVERRIDE {
  }

  virtual void OnProfileAvatarChanged(
      const base::FilePath& profile_path) OVERRIDE {
  }

  ProfileManager* profile_manager_;

  AppController* app_controller_;  // Weak; owns us.

  DISALLOW_COPY_AND_ASSIGN(AppControllerProfileObserver);
};

@implementation AppController

@synthesize startupComplete = startupComplete_;

// This method is called very early in application startup (ie, before
// the profile is loaded or any preferences have been registered). Defer any
// user-data initialization until -applicationDidFinishLaunching:.
- (void)awakeFromNib {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::AWAKE_FROM_NIB);
  // We need to register the handlers early to catch events fired on launch.
  NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
  [em setEventHandler:self
          andSelector:@selector(getUrl:withReply:)
        forEventClass:kInternetEventClass
           andEventID:kAEGetURL];
  [em setEventHandler:self
          andSelector:@selector(getUrl:withReply:)
        forEventClass:'WWW!'    // A particularly ancient AppleEvent that dates
           andEventID:'OURL'];  // back to the Spyglass days.

  // Register for various window layering changes. We use these to update
  // various UI elements (command-key equivalents, etc) when the frontmost
  // window changes.
  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];
  [notificationCenter
      addObserver:self
         selector:@selector(windowLayeringDidChange:)
             name:NSWindowDidBecomeKeyNotification
           object:nil];
  [notificationCenter
      addObserver:self
         selector:@selector(windowLayeringDidChange:)
             name:NSWindowDidResignKeyNotification
           object:nil];
  [notificationCenter
      addObserver:self
         selector:@selector(windowLayeringDidChange:)
             name:NSWindowDidBecomeMainNotification
           object:nil];
  [notificationCenter
      addObserver:self
         selector:@selector(windowLayeringDidChange:)
             name:NSWindowDidResignMainNotification
           object:nil];

  if (base::mac::IsOSLionOrLater()) {
    [notificationCenter
        addObserver:self
           selector:@selector(popoverDidShow:)
               name:NSPopoverDidShowNotification
             object:nil];
    [notificationCenter
        addObserver:self
           selector:@selector(popoverDidClose:)
               name:NSPopoverDidCloseNotification
             object:nil];
  }

  // Register for space change notifications.
  [[[NSWorkspace sharedWorkspace] notificationCenter]
    addObserver:self
       selector:@selector(activeSpaceDidChange:)
           name:NSWorkspaceActiveSpaceDidChangeNotification
         object:nil];

  // Set up the command updater for when there are no windows open
  [self initMenuState];

  // Initialize the Profile menu.
  [self initProfileMenu];
}

- (void)unregisterEventHandlers {
  NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
  [em removeEventHandlerForEventClass:kInternetEventClass
                           andEventID:kAEGetURL];
  [em removeEventHandlerForEventClass:cloud_print::kAECloudPrintClass
                           andEventID:cloud_print::kAECloudPrintClass];
  [em removeEventHandlerForEventClass:'WWW!'
                           andEventID:'OURL'];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
}

// (NSApplicationDelegate protocol) This is the Apple-approved place to override
// the default handlers.
- (void)applicationWillFinishLaunching:(NSNotification*)notification {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::WILL_FINISH_LAUNCHING);
}

- (void)applicationWillHide:(NSNotification*)notification {
  apps::ExtensionAppShimHandler::OnChromeWillHide();
}

- (BOOL)tryToTerminateApplication:(NSApplication*)app {
  // Check for in-process downloads, and prompt the user if they really want
  // to quit (and thus cancel downloads). Only check if we're not already
  // shutting down, else the user might be prompted multiple times if the
  // download isn't stopped before terminate is called again.
  if (!browser_shutdown::IsTryingToQuit() &&
      ![self shouldQuitWithInProgressDownloads])
    return NO;

  // TODO(viettrungluu): Remove Apple Event handlers here? (It's safe to leave
  // them in, but I'm not sure about UX; we'd also want to disable other things
  // though.) http://crbug.com/40861

  // Check if the user really wants to quit by employing the confirm-to-quit
  // mechanism.
  if (!browser_shutdown::IsTryingToQuit() &&
      [self applicationShouldTerminate:app] != NSTerminateNow)
    return NO;

  // Check for active apps. If quitting is prevented, only close browsers and
  // sessions.
  if (!browser_shutdown::IsTryingToQuit() &&
      quitWithAppsController_ && !quitWithAppsController_->ShouldQuit()) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
    // This will close all browser sessions.
    chrome::CloseAllBrowsers();
    return NO;
  }

  size_t num_browsers = chrome::GetTotalBrowserCount();

  // Initiate a shutdown (via chrome::CloseAllBrowsersAndQuit()) if we aren't
  // already shutting down.
  if (!browser_shutdown::IsTryingToQuit()) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
    chrome::CloseAllBrowsersAndQuit();
  }

  return num_browsers == 0 ? YES : NO;
}

- (void)stopTryingToTerminateApplication:(NSApplication*)app {
  if (browser_shutdown::IsTryingToQuit()) {
    // Reset the "trying to quit" state, so that closing all browser windows
    // will no longer lead to termination.
    browser_shutdown::SetTryingToQuit(false);

    // TODO(viettrungluu): Were we to remove Apple Event handlers above, we
    // would have to reinstall them here. http://crbug.com/40861
  }
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)app {
  // If there are no windows, quit immediately.
  if (chrome::BrowserIterator().done() &&
      !apps::AppWindowRegistry::IsAppWindowRegisteredInAnyProfile(0)) {
    return NSTerminateNow;
  }

  // Check if the preference is turned on.
  const PrefService* prefs = g_browser_process->local_state();
  if (!prefs->GetBoolean(prefs::kConfirmToQuitEnabled)) {
    confirm_quit::RecordHistogram(confirm_quit::kNoConfirm);
    return NSTerminateNow;
  }

  // If the application is going to terminate as the result of a Cmd+Q
  // invocation, use the special sauce to prevent accidental quitting.
  // http://dev.chromium.org/developers/design-documents/confirm-to-quit-experiment

  // This logic is only for keyboard-initiated quits.
  if (![ConfirmQuitPanelController eventTriggersFeature:[app currentEvent]])
    return NSTerminateNow;

  return [[ConfirmQuitPanelController sharedController]
      runModalLoopForApplication:app];
}

// Called when the app is shutting down. Clean-up as appropriate.
- (void)applicationWillTerminate:(NSNotification*)aNotification {
  // There better be no browser windows left at this point.
  CHECK_EQ(0u, chrome::GetTotalBrowserCount());

  // Tell BrowserList not to keep the browser process alive. Once all the
  // browsers get dealloc'd, it will stop the RunLoop and fall back into main().
  chrome::DecrementKeepAliveCount();

  // Reset all pref watching, as this object outlives the prefs system.
  profilePrefRegistrar_.reset();
  localPrefRegistrar_.RemoveAll();

  [self unregisterEventHandlers];

  appShimMenuController_.reset();
}

- (void)didEndMainMessageLoop {
  DCHECK_EQ(0u, chrome::GetBrowserCount([self lastProfile],
                                        chrome::HOST_DESKTOP_TYPE_NATIVE));
  if (!chrome::GetBrowserCount([self lastProfile],
                               chrome::HOST_DESKTOP_TYPE_NATIVE)) {
    // As we're shutting down, we need to nuke the TabRestoreService, which
    // will start the shutdown of the NavigationControllers and allow for
    // proper shutdown. If we don't do this, Chrome won't shut down cleanly,
    // and may end up crashing when some thread tries to use the IO thread (or
    // another thread) that is no longer valid.
    TabRestoreServiceFactory::ResetForProfile([self lastProfile]);
  }
}

// If the window has a tab controller, make "close window" be cmd-shift-w,
// otherwise leave it as the normal cmd-w. Capitalization of the key equivalent
// affects whether the shift modifier is used.
- (void)adjustCloseWindowMenuItemKeyEquivalent:(BOOL)enableCloseTabShortcut {
  [closeWindowMenuItem_ setKeyEquivalent:(enableCloseTabShortcut ? @"W" :
                                                                   @"w")];
  [closeWindowMenuItem_ setKeyEquivalentModifierMask:NSCommandKeyMask];
}

// If the window has a tab controller, make "close tab" take over cmd-w,
// otherwise it shouldn't have any key-equivalent because it should be disabled.
- (void)adjustCloseTabMenuItemKeyEquivalent:(BOOL)enableCloseTabShortcut {
  if (enableCloseTabShortcut) {
    [closeTabMenuItem_ setKeyEquivalent:@"w"];
    [closeTabMenuItem_ setKeyEquivalentModifierMask:NSCommandKeyMask];
  } else {
    [closeTabMenuItem_ setKeyEquivalent:@""];
    [closeTabMenuItem_ setKeyEquivalentModifierMask:0];
  }
}

// Explicitly remove any command-key equivalents from the close tab/window
// menus so that nothing can go haywire if we get a user action during pending
// updates.
- (void)clearCloseMenuItemKeyEquivalents {
  [closeTabMenuItem_ setKeyEquivalent:@""];
  [closeTabMenuItem_ setKeyEquivalentModifierMask:0];
  [closeWindowMenuItem_ setKeyEquivalent:@""];
  [closeWindowMenuItem_ setKeyEquivalentModifierMask:0];
}

// See if the focused window window has tabs, and adjust the key equivalents for
// Close Tab/Close Window accordingly.
- (void)fixCloseMenuItemKeyEquivalents {
  fileMenuUpdatePending_ = NO;

  NSWindow* window = [NSApp keyWindow];
  NSWindow* mainWindow = [NSApp mainWindow];
  if (!window || ([window parentWindow] == mainWindow)) {
    // If the key window is a child of the main window (e.g. a bubble), the main
    // window should be the one that handles the close menu item action.
    // Also, there might be a small amount of time where there is no key window;
    // in that case as well, just use our main browser window if there is one.
    // You might think that we should just always use the main window, but the
    // "About Chrome" window serves as a counterexample.
    window = mainWindow;
  }

  BOOL hasTabs =
      [[window windowController] isKindOfClass:[TabWindowController class]];
  BOOL enableCloseTabShortcut = hasTabs && !hasPopover_;
  [self adjustCloseWindowMenuItemKeyEquivalent:enableCloseTabShortcut];
  [self adjustCloseTabMenuItemKeyEquivalent:enableCloseTabShortcut];
}

// Fix up the "close tab/close window" command-key equivalents. We do this
// after a delay to ensure that window layer state has been set by the time
// we do the enabling. This should only be called on the main thread, code that
// calls this (even as a side-effect) from other threads needs to be fixed.
- (void)delayedFixCloseMenuItemKeyEquivalents {
  DCHECK([NSThread isMainThread]);
  if (!fileMenuUpdatePending_) {
    // The OS prefers keypresses to timers, so it's possible that a cmd-w
    // can sneak in before this timer fires. In order to prevent that from
    // having any bad consequences, just clear the keys combos altogether. They
    // will be reset when the timer eventually fires.
    if ([NSThread isMainThread]) {
      fileMenuUpdatePending_ = YES;
      [self clearCloseMenuItemKeyEquivalents];
      [self performSelector:@selector(fixCloseMenuItemKeyEquivalents)
                 withObject:nil
                 afterDelay:0];
    } else {
      // This shouldn't be happening, but if it does, force it to the main
      // thread to avoid dropping the update. Don't mess with
      // |fileMenuUpdatePending_| as it's not expected to be threadsafe and
      // there could be a race between the selector finishing and setting the
      // flag.
      [self
          performSelectorOnMainThread:@selector(fixCloseMenuItemKeyEquivalents)
                           withObject:nil
                        waitUntilDone:NO];
    }
  }
}

// Called when we get a notification about the window layering changing to
// update the UI based on the new main window.
- (void)windowLayeringDidChange:(NSNotification*)notify {
  [self delayedFixCloseMenuItemKeyEquivalents];

  if ([notify name] == NSWindowDidResignKeyNotification) {
    // If a window is closed, this notification is fired but |[NSApp keyWindow]|
    // returns nil regardless of whether any suitable candidates for the key
    // window remain. It seems that the new key window for the app is not set
    // until after this notification is fired, so a check is performed after the
    // run loop is allowed to spin.
    [self performSelector:@selector(checkForAnyKeyWindows)
               withObject:nil
               afterDelay:0.0];
  }

  // If the window changed to a new BrowserWindowController, update the profile.
  id windowController = [[notify object] windowController];
  if (![windowController isKindOfClass:[BrowserWindowController class]])
    return;

  if ([notify name] == NSWindowDidBecomeMainNotification) {
    // If the profile is incognito, use the original profile.
    Profile* newProfile = [windowController profile]->GetOriginalProfile();
    [self windowChangedToProfile:newProfile];
  } else if (chrome::GetTotalBrowserCount() == 0) {
    [self windowChangedToProfile:
        g_browser_process->profile_manager()->GetLastUsedProfile()];
  }
}

- (void)activeSpaceDidChange:(NSNotification*)notify {
  if (reopenTime_.is_null() ||
      ![NSApp isActive] ||
      (base::TimeTicks::Now() - reopenTime_).InMilliseconds() >
      kWorkspaceChangeTimeoutMs) {
    return;
  }

  // The last applicationShouldHandleReopen:hasVisibleWindows: call
  // happened during a space change. Now that the change has
  // completed, raise browser windows.
  reopenTime_ = base::TimeTicks();
  std::set<NSWindow*> browserWindows;
  for (chrome::BrowserIterator iter; !iter.done(); iter.Next()) {
    Browser* browser = *iter;
    browserWindows.insert(browser->window()->GetNativeWindow());
  }
  if (!browserWindows.empty()) {
    ui::FocusWindowSetOnCurrentSpace(browserWindows);
  }
}

// Called on Lion and later when a popover (e.g. dictionary) is shown.
- (void)popoverDidShow:(NSNotification*)notify {
  hasPopover_ = YES;
  [self fixCloseMenuItemKeyEquivalents];
}

// Called on Lion and later when a popover (e.g. dictionary) is closed.
- (void)popoverDidClose:(NSNotification*)notify {
  hasPopover_ = NO;
  [self fixCloseMenuItemKeyEquivalents];
}

// Called when the user has changed browser windows, meaning the backing profile
// may have changed. This can cause a rebuild of the user-data menus. This is a
// no-op if the new profile is the same as the current one. This will always be
// the original profile and never incognito.
- (void)windowChangedToProfile:(Profile*)profile {
  if (lastProfile_ == profile)
    return;

  // Before tearing down the menu controller bridges, return the Cocoa menus to
  // their initial state.
  if (bookmarkMenuBridge_.get())
    bookmarkMenuBridge_->ResetMenu();
  if (historyMenuBridge_.get())
    historyMenuBridge_->ResetMenu();

  // Rebuild the menus with the new profile.
  lastProfile_ = profile;

  bookmarkMenuBridge_.reset(new BookmarkMenuBridge(lastProfile_,
      [[[NSApp mainMenu] itemWithTag:IDC_BOOKMARKS_MENU] submenu]));
  // No need to |BuildMenu| here.  It is done lazily upon menu access.

  historyMenuBridge_.reset(new HistoryMenuBridge(lastProfile_));
  historyMenuBridge_->BuildMenu();

  chrome::BrowserCommandController::
      UpdateSharedCommandsForIncognitoAvailability(
          menuState_.get(), lastProfile_);
  profilePrefRegistrar_.reset(new PrefChangeRegistrar());
  profilePrefRegistrar_->Init(lastProfile_->GetPrefs());
  profilePrefRegistrar_->Add(
      prefs::kIncognitoModeAvailability,
      base::Bind(&chrome::BrowserCommandController::
                     UpdateSharedCommandsForIncognitoAvailability,
                 menuState_.get(),
                 lastProfile_));
}

- (void)checkForAnyKeyWindows {
  if ([NSApp keyWindow])
    return;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_NO_KEY_WINDOW,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

// If the auto-update interval is not set, make it 5 hours.
// Placed here for 2 reasons:
// 1) Same spot as other Pref stuff
// 2) Try and be friendly by keeping this after app launch
- (void)setUpdateCheckInterval {
#if defined(GOOGLE_CHROME_BUILD)
  CFStringRef app = CFSTR("com.google.Keystone.Agent");
  CFStringRef checkInterval = CFSTR("checkInterval");
  CFPropertyListRef plist = CFPreferencesCopyAppValue(checkInterval, app);
  if (!plist) {
    const float fiveHoursInSeconds = 5.0 * 60.0 * 60.0;
    NSNumber* value = [NSNumber numberWithFloat:fiveHoursInSeconds];
    CFPreferencesSetAppValue(checkInterval, value, app);
    CFPreferencesAppSynchronize(app);
  }
#endif
}

- (void)openStartupUrls {
  // On Mac, the URLs are passed in via Cocoa, not command line. The Chrome
  // NSApplication is created in MainMessageLoop, and then the shortcut urls
  // are passed in via Apple events. At this point, the first browser is
  // already loaded in PreMainMessageLoop. If we initialize NSApplication
  // before PreMainMessageLoop to capture shortcut URL events, it may cause
  // more problems because it relies on things created in PreMainMessageLoop
  // and may break existing message loop design.
  if (startupUrls_.empty())
    return;

  // If there's only 1 tab and the tab is NTP, close this NTP tab and open all
  // startup urls in new tabs, because the omnibox will stay focused if we
  // load url in NTP tab.
  Browser* browser = chrome::GetLastActiveBrowser();
  int startupIndex = TabStripModel::kNoTab;
  content::WebContents* startupContent = NULL;

  if (browser && browser->tab_strip_model()->count() == 1) {
    startupIndex = browser->tab_strip_model()->active_index();
    startupContent = browser->tab_strip_model()->GetActiveWebContents();
  }

  if (startupUrls_.size()) {
    [self openUrls:startupUrls_];
    startupUrls_.clear();
  }

  if (startupIndex != TabStripModel::kNoTab &&
      startupContent->GetVisibleURL() == GURL(chrome::kChromeUINewTabURL)) {
    browser->tab_strip_model()->CloseWebContentsAt(startupIndex,
        TabStripModel::CLOSE_NONE);
  }
}

// This is called after profiles have been loaded and preferences registered.
// It is safe to access the default profile here.
- (void)applicationDidFinishLaunching:(NSNotification*)notify {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::DID_FINISH_LAUNCHING);
  MacStartupProfiler::GetInstance()->RecordMetrics();

  // Notify BrowserList to keep the application running so it doesn't go away
  // when all the browser windows get closed.
  chrome::IncrementKeepAliveCount();

  [self setUpdateCheckInterval];

  // Start managing the menu for app windows. This needs to be done here because
  // main menu item titles are not yet initialized in awakeFromNib.
  [self initAppShimMenuController];

  // If enabled, keep Chrome alive when apps are open instead of quitting all
  // apps.
  quitWithAppsController_ = new QuitWithAppsController();

  // Build up the encoding menu, the order of the items differs based on the
  // current locale (see http://crbug.com/7647 for details).
  // We need a valid g_browser_process to get the profile which is why we can't
  // call this from awakeFromNib.
  NSMenu* viewMenu = [[[NSApp mainMenu] itemWithTag:IDC_VIEW_MENU] submenu];
  NSMenuItem* encodingMenuItem = [viewMenu itemWithTag:IDC_ENCODING_MENU];
  NSMenu* encodingMenu = [encodingMenuItem submenu];
  EncodingMenuControllerDelegate::BuildEncodingMenu([self lastProfile],
                                                    encodingMenu);

  // Instantiate the ProfileInfoCache observer so that we can get
  // notified when a profile is deleted.
  profileInfoCacheObserver_.reset(new AppControllerProfileObserver(
      g_browser_process->profile_manager(), self));

  // Since Chrome is localized to more languages than the OS, tell Cocoa which
  // menu is the Help so it can add the search item to it.
  [NSApp setHelpMenu:helpMenu_];

  // Record the path to the (browser) app bundle; this is used by the app mode
  // shim.  It has to be done in FILE thread because getting the path requires
  // I/O.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&RecordLastRunAppBundlePath));

  // Makes "Services" menu items available.
  [self registerServicesMenuTypesTo:[notify object]];

  startupComplete_ = YES;

  [self openStartupUrls];

  PrefService* localState = g_browser_process->local_state();
  if (localState) {
    localPrefRegistrar_.Init(localState);
    localPrefRegistrar_.Add(
        prefs::kAllowFileSelectionDialogs,
        base::Bind(&chrome::BrowserCommandController::UpdateOpenFileState,
                   menuState_.get()));
  }
}

// This is called after profiles have been loaded and preferences registered.
// It is safe to access the default profile here.
- (void)applicationDidBecomeActive:(NSNotification*)notify {
  content::PluginService::GetInstance()->AppActivated();
}

// Helper function for populating and displaying the in progress downloads at
// exit alert panel.
- (BOOL)userWillWaitForInProgressDownloads:(int)downloadCount {
  NSString* titleText = nil;
  NSString* explanationText = nil;
  NSString* waitTitle = nil;
  NSString* exitTitle = nil;

  // Set the dialog text based on whether or not there are multiple downloads.
  if (downloadCount == 1) {
    // Dialog text: warning and explanation.
    titleText = l10n_util::GetNSString(
        IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_TITLE);
    explanationText = l10n_util::GetNSString(
        IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_EXPLANATION);
  } else {
    // Dialog text: warning and explanation.
    titleText = l10n_util::GetNSStringF(
        IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_TITLE,
        base::IntToString16(downloadCount));
    explanationText = l10n_util::GetNSString(
        IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_EXPLANATION);
  }
  // Cancel download and exit button text.
  exitTitle = l10n_util::GetNSString(
      IDS_DOWNLOAD_REMOVE_CONFIRM_OK_BUTTON_LABEL);

  // Wait for download button text.
  waitTitle = l10n_util::GetNSString(
      IDS_DOWNLOAD_REMOVE_CONFIRM_CANCEL_BUTTON_LABEL);

  // 'waitButton' is the default choice.
  int choice = NSRunAlertPanel(titleText, @"%@",
                               waitTitle, exitTitle, nil, explanationText);
  return choice == NSAlertDefaultReturn ? YES : NO;
}

// Check all profiles for in progress downloads, and if we find any, prompt the
// user to see if we should continue to exit (and thus cancel the downloads), or
// if we should wait.
- (BOOL)shouldQuitWithInProgressDownloads {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return YES;

  std::vector<Profile*> profiles(profile_manager->GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i) {
    DownloadService* download_service =
      DownloadServiceFactory::GetForBrowserContext(profiles[i]);
    DownloadManager* download_manager =
        (download_service->HasCreatedDownloadManager() ?
         BrowserContext::GetDownloadManager(profiles[i]) : NULL);
    if (download_manager &&
        download_manager->NonMaliciousInProgressCount() > 0) {
      int downloadCount = download_manager->NonMaliciousInProgressCount();
      if ([self userWillWaitForInProgressDownloads:downloadCount]) {
        // Create a new browser window (if necessary) and navigate to the
        // downloads page if the user chooses to wait.
        Browser* browser = chrome::FindBrowserWithProfile(
            profiles[i], chrome::HOST_DESKTOP_TYPE_NATIVE);
        if (!browser) {
          browser = new Browser(Browser::CreateParams(
              profiles[i], chrome::HOST_DESKTOP_TYPE_NATIVE));
          browser->window()->Show();
        }
        DCHECK(browser);
        chrome::ShowDownloads(browser);
        return NO;
      }

      // User wants to exit.
      return YES;
    }
  }

  // No profiles or active downloads found, okay to exit.
  return YES;
}

// Called to determine if we should enable the "restore tab" menu item.
// Checks with the TabRestoreService to see if there's anything there to
// restore and returns YES if so.
- (BOOL)canRestoreTab {
  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile([self lastProfile]);
  return service && !service->entries().empty();
}

// Called from the AppControllerProfileObserver every time a profile is deleted.
- (void)profileWasRemoved:(const base::FilePath&)profilePath {
  Profile* lastProfile = [self lastProfile];

  // If the lastProfile has been deleted, the profile manager has
  // already loaded a new one, so the pointer needs to be updated;
  // otherwise we will try to start up a browser window with a pointer
  // to the old profile.
  if (profilePath == lastProfile->GetPath())
    lastProfile_ = g_browser_process->profile_manager()->GetLastUsedProfile();
}

// Returns true if there is a modal window (either window- or application-
// modal) blocking the active browser. Note that tab modal dialogs (HTTP auth
// sheets) will not count as blocking the browser. But things like open/save
// dialogs that are window modal will block the browser.
- (BOOL)keyWindowIsModal {
  if ([NSApp modalWindow])
    return YES;

  Browser* browser = chrome::GetLastActiveBrowser();
  return browser &&
         [[browser->window()->GetNativeWindow() attachedSheet]
             isKindOfClass:[NSWindow class]];
}

// Called to validate menu items when there are no key windows. All the
// items we care about have been set with the |commandDispatch:| action and
// a target of FirstResponder in IB. If it's not one of those, let it
// continue up the responder chain to be handled elsewhere. We pull out the
// tag as the cross-platform constant to differentiate and dispatch the
// various commands.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];
  BOOL enable = NO;
  if (action == @selector(commandDispatch:) ||
      action == @selector(commandFromDock:)) {
    NSInteger tag = [item tag];
    if (menuState_ &&  // NULL in tests.
        menuState_->SupportsCommand(tag)) {
      switch (tag) {
        // The File Menu commands are not automatically disabled by Cocoa when a
        // dialog sheet obscures the browser window, so we disable several of
        // them here.  We don't need to include IDC_CLOSE_WINDOW, because
        // app_controller is only activated when there are no key windows (see
        // function comment).
        case IDC_RESTORE_TAB:
          enable = ![self keyWindowIsModal] && [self canRestoreTab];
          break;
        // Browser-level items that open in new tabs should not open if there's
        // a window- or app-modal dialog.
        case IDC_OPEN_FILE:
        case IDC_NEW_TAB:
        case IDC_SHOW_HISTORY:
        case IDC_SHOW_BOOKMARK_MANAGER:
          enable = ![self keyWindowIsModal];
          break;
        // Browser-level items that open in new windows.
        case IDC_TASK_MANAGER:
          // Allow the user to open a new window if there's a window-modal
          // dialog.
          enable = ![self keyWindowIsModal];
          break;
        case IDC_SHOW_SYNC_SETUP: {
          Profile* lastProfile = [self lastProfile];
          // The profile may be NULL during shutdown -- see
          // http://code.google.com/p/chromium/issues/detail?id=43048 .
          //
          // TODO(akalin,viettrungluu): Figure out whether this method
          // can be prevented from being called if lastProfile is
          // NULL.
          if (!lastProfile) {
            LOG(WARNING)
                << "NULL lastProfile detected -- not doing anything";
            break;
          }
          SigninManager* signin = SigninManagerFactory::GetForProfile(
              lastProfile->GetOriginalProfile());
          enable = signin->IsSigninAllowed() &&
              ![self keyWindowIsModal];
          [BrowserWindowController updateSigninItem:item
                                         shouldShow:enable
                                     currentProfile:lastProfile];
          break;
        }
#if defined(GOOGLE_CHROME_BUILD)
        case IDC_FEEDBACK:
          enable = NO;
          break;
#endif
        default:
          enable = menuState_->IsCommandEnabled(tag) ?
                   ![self keyWindowIsModal] : NO;
      }
    }
  } else if (action == @selector(terminate:)) {
    enable = YES;
  } else if (action == @selector(showPreferences:)) {
    enable = YES;
  } else if (action == @selector(orderFrontStandardAboutPanel:)) {
    enable = YES;
  } else if (action == @selector(commandFromDock:)) {
    enable = YES;
  } else if (action == @selector(toggleConfirmToQuit:)) {
    [self updateConfirmToQuitPrefMenuItem:static_cast<NSMenuItem*>(item)];
    enable = YES;
  } else if (action == @selector(toggleDisplayMessageCenter:)) {
    NSMenuItem* menuItem = static_cast<NSMenuItem*>(item);
    [self updateDisplayMessageCenterPrefMenuItem:menuItem];
    enable = YES;
  } else if (action == @selector(executeApplication:)) {
    enable = YES;
  }
  return enable;
}

// Called when the user picks a menu item when there are no key windows, or when
// there is no foreground browser window. Calls through to the browser object to
// execute the command. This assumes that the command is supported and doesn't
// check, otherwise it should have been disabled in the UI in
// |-validateUserInterfaceItem:|.
- (void)commandDispatch:(id)sender {
  Profile* lastProfile = [self safeLastProfileForNewWindows];

  // Handle the case where we're dispatching a command from a sender that's in a
  // browser window. This means that the command came from a background window
  // and is getting here because the foreground window is not a browser window.
  if ([sender respondsToSelector:@selector(window)]) {
    id delegate = [[sender window] windowController];
    if ([delegate isKindOfClass:[BrowserWindowController class]]) {
      [delegate commandDispatch:sender];
      return;
    }
  }

  // Ignore commands during session restore's browser creation.  It uses a
  // nested message loop and commands dispatched during this operation cause
  // havoc.
  if (SessionRestore::IsRestoring(lastProfile) &&
      base::MessageLoop::current()->IsNested())
    return;

  NSInteger tag = [sender tag];

  // If there are no browser windows, and we are trying to open a browser
  // for a locked profile, we have to show the User Manager instead as the
  // locked profile needs authentication.
  if (IsProfileSignedOut(lastProfile)) {
    chrome::ShowUserManager(lastProfile->GetPath());
    return;
  }

  switch (tag) {
    case IDC_NEW_TAB:
      // Create a new tab in an existing browser window (which we activate) if
      // possible.
      if (Browser* browser = ActivateBrowser(lastProfile)) {
        chrome::ExecuteCommand(browser, IDC_NEW_TAB);
        break;
      }
      // Else fall through to create new window.
    case IDC_NEW_WINDOW:
      CreateBrowser(lastProfile);
      break;
    case IDC_FOCUS_LOCATION:
      chrome::ExecuteCommand(ActivateOrCreateBrowser(lastProfile),
                             IDC_FOCUS_LOCATION);
      break;
    case IDC_FOCUS_SEARCH:
      chrome::ExecuteCommand(ActivateOrCreateBrowser(lastProfile),
                             IDC_FOCUS_SEARCH);
      break;
    case IDC_NEW_INCOGNITO_WINDOW:
      CreateBrowser(lastProfile->GetOffTheRecordProfile());
      break;
    case IDC_RESTORE_TAB:
      // There is only the native desktop on Mac.
      chrome::OpenWindowWithRestoredTabs(lastProfile,
                                         chrome::HOST_DESKTOP_TYPE_NATIVE);
      break;
    case IDC_OPEN_FILE:
      chrome::ExecuteCommand(CreateBrowser(lastProfile), IDC_OPEN_FILE);
      break;
    case IDC_CLEAR_BROWSING_DATA: {
      // There may not be a browser open, so use the default profile.
      if (Browser* browser = ActivateBrowser(lastProfile)) {
        chrome::ShowClearBrowsingDataDialog(browser);
      } else {
        chrome::OpenClearBrowsingDataDialogWindow(lastProfile);
      }
      break;
    }
    case IDC_IMPORT_SETTINGS: {
      if (Browser* browser = ActivateBrowser(lastProfile)) {
        chrome::ShowImportDialog(browser);
      } else {
        chrome::OpenImportSettingsDialogWindow(lastProfile);
      }
      break;
    }
    case IDC_SHOW_BOOKMARK_MANAGER:
      content::RecordAction(UserMetricsAction("ShowBookmarkManager"));
      if (Browser* browser = ActivateBrowser(lastProfile)) {
        chrome::ShowBookmarkManager(browser);
      } else {
        // No browser window, so create one for the bookmark manager tab.
        chrome::OpenBookmarkManagerWindow(lastProfile);
      }
      break;
    case IDC_SHOW_HISTORY:
      if (Browser* browser = ActivateBrowser(lastProfile))
        chrome::ShowHistory(browser);
      else
        chrome::OpenHistoryWindow(lastProfile);
      break;
    case IDC_SHOW_DOWNLOADS:
      if (Browser* browser = ActivateBrowser(lastProfile))
        chrome::ShowDownloads(browser);
      else
        chrome::OpenDownloadsWindow(lastProfile);
      break;
    case IDC_MANAGE_EXTENSIONS:
      if (Browser* browser = ActivateBrowser(lastProfile))
        chrome::ShowExtensions(browser, std::string());
      else
        chrome::OpenExtensionsWindow(lastProfile);
      break;
    case IDC_HELP_PAGE_VIA_MENU:
      if (Browser* browser = ActivateBrowser(lastProfile))
        chrome::ShowHelp(browser, chrome::HELP_SOURCE_MENU);
      else
        chrome::OpenHelpWindow(lastProfile, chrome::HELP_SOURCE_MENU);
      break;
    case IDC_SHOW_SYNC_SETUP:
      if (Browser* browser = ActivateBrowser(lastProfile)) {
        chrome::ShowBrowserSignin(browser, signin::SOURCE_MENU);
      } else {
        chrome::OpenSyncSetupWindow(lastProfile, signin::SOURCE_MENU);
      }
      break;
    case IDC_TASK_MANAGER:
      content::RecordAction(UserMetricsAction("TaskManager"));
      TaskManagerMac::Show();
      break;
    case IDC_OPTIONS:
      [self showPreferences:sender];
      break;
  }
}

// Run a (background) application in a new tab.
- (void)executeApplication:(id)sender {
  NSInteger tag = [sender tag];
  Profile* profile = [self lastProfile];
  DCHECK(profile);
  BackgroundApplicationListModel applications(profile);
  DCHECK(tag >= 0 &&
         tag < static_cast<int>(applications.size()));
  const extensions::Extension* extension = applications.GetExtension(tag);
  BackgroundModeManager::LaunchBackgroundApplication(profile, extension);
}

// Same as |-commandDispatch:|, but executes commands using a disposition
// determined by the key flags. This will get called in the case where the
// frontmost window is not a browser window, and the user has command-clicked
// a button in a background browser window whose action is
// |-commandDispatchUsingKeyModifiers:|
- (void)commandDispatchUsingKeyModifiers:(id)sender {
  DCHECK(sender);
  if ([sender respondsToSelector:@selector(window)]) {
    id delegate = [[sender window] windowController];
    if ([delegate isKindOfClass:[BrowserWindowController class]]) {
      [delegate commandDispatchUsingKeyModifiers:sender];
    }
  }
}

// NSApplication delegate method called when someone clicks on the dock icon.
// To match standard mac behavior, we should open a new window if there are no
// browser windows.
- (BOOL)applicationShouldHandleReopen:(NSApplication*)theApplication
                    hasVisibleWindows:(BOOL)hasVisibleWindows {
  // If the browser is currently trying to quit, don't do anything and return NO
  // to prevent AppKit from doing anything.
  // TODO(rohitrao): Remove this code when http://crbug.com/40861 is resolved.
  if (browser_shutdown::IsTryingToQuit())
    return NO;

  // Bring all browser windows to the front. Specifically, this brings them in
  // front of any app windows. FocusWindowSet will also unminimize the most
  // recently minimized window if no windows in the set are visible.
  // If there are any, return here. Otherwise, the windows are panels or
  // notifications so we still need to open a new window.
  if (hasVisibleWindows) {
    std::set<NSWindow*> browserWindows;
    for (chrome::BrowserIterator iter; !iter.done(); iter.Next()) {
      Browser* browser = *iter;
      browserWindows.insert(browser->window()->GetNativeWindow());
    }
    if (!browserWindows.empty()) {
      NSWindow* keyWindow = [NSApp keyWindow];
      if (keyWindow && ![keyWindow isOnActiveSpace]) {
        // The key window is not on the active space. We must be mid-animation
        // for a space transition triggered by the dock. Delay the call to
        // |ui::FocusWindowSet| until the transition completes. Otherwise, the
        // wrong space's windows get raised, resulting in an off-screen key
        // window. It does not work to |ui::FocusWindowSet| twice, once here
        // and once in |activeSpaceDidChange:|, as that appears to break when
        // the omnibox is focused.
        //
        // This check relies on OS X setting the key window to a window on the
        // target space before calling this method.
        //
        // See http://crbug.com/309656.
        reopenTime_ = base::TimeTicks::Now();
      } else {
        ui::FocusWindowSetOnCurrentSpace(browserWindows);
      }
      // Return NO; we've done (or soon will do) the deminiaturize, so
      // AppKit shouldn't do anything.
      return NO;
    }
  }

  // If launched as a hidden login item (due to installation of a persistent app
  // or by the user, for example in System Preferences->Accounts->Login Items),
  // allow session to be restored first time the user clicks on a Dock icon.
  // Normally, it'd just open a new empty page.
  {
    static BOOL doneOnce = NO;
    BOOL attemptRestore = apps::AppShimHandler::ShouldRestoreSession() ||
        (!doneOnce && base::mac::WasLaunchedAsHiddenLoginItem());
    doneOnce = YES;
    if (attemptRestore) {
      SessionService* sessionService =
          SessionServiceFactory::GetForProfileForSessionRestore(
              [self lastProfile]);
      if (sessionService &&
          sessionService->RestoreIfNecessary(std::vector<GURL>()))
        return NO;
    }
  }

  // Otherwise open a new window.
  // If the last profile was locked, we have to open the User Manager, as the
  // profile requires authentication. Similarly, because guest mode is
  // implemented as forced incognito, we can't open a new guest browser either,
  // so we have to show the User Manager as well.
  Profile* lastProfile = [self lastProfile];
  if (lastProfile->IsGuestSession() || IsProfileSignedOut(lastProfile))
    chrome::ShowUserManager(lastProfile->GetPath());
  else
    CreateBrowser(lastProfile);

  // We've handled the reopen event, so return NO to tell AppKit not
  // to do anything.
  return NO;
}

- (void)initMenuState {
  menuState_.reset(new CommandUpdater(NULL));
  menuState_->UpdateCommandEnabled(IDC_NEW_TAB, true);
  menuState_->UpdateCommandEnabled(IDC_NEW_WINDOW, true);
  menuState_->UpdateCommandEnabled(IDC_NEW_INCOGNITO_WINDOW, true);
  menuState_->UpdateCommandEnabled(IDC_OPEN_FILE, true);
  menuState_->UpdateCommandEnabled(IDC_CLEAR_BROWSING_DATA, true);
  menuState_->UpdateCommandEnabled(IDC_RESTORE_TAB, false);
  menuState_->UpdateCommandEnabled(IDC_FOCUS_LOCATION, true);
  menuState_->UpdateCommandEnabled(IDC_FOCUS_SEARCH, true);
  menuState_->UpdateCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER, true);
  menuState_->UpdateCommandEnabled(IDC_SHOW_HISTORY, true);
  menuState_->UpdateCommandEnabled(IDC_SHOW_DOWNLOADS, true);
  menuState_->UpdateCommandEnabled(IDC_MANAGE_EXTENSIONS, true);
  menuState_->UpdateCommandEnabled(IDC_HELP_PAGE_VIA_MENU, true);
  menuState_->UpdateCommandEnabled(IDC_IMPORT_SETTINGS, true);
#if defined(GOOGLE_CHROME_BUILD)
  menuState_->UpdateCommandEnabled(IDC_FEEDBACK, true);
#endif
  menuState_->UpdateCommandEnabled(IDC_SHOW_SYNC_SETUP, true);
  menuState_->UpdateCommandEnabled(IDC_TASK_MANAGER, true);
}

// Conditionally adds the Profile menu to the main menu bar.
- (void)initProfileMenu {
  NSMenu* mainMenu = [NSApp mainMenu];
  NSMenuItem* profileMenu = [mainMenu itemWithTag:IDC_PROFILE_MAIN_MENU];

  if (!profiles::IsMultipleProfilesEnabled()) {
    [mainMenu removeItem:profileMenu];
    return;
  }

  // The controller will unhide the menu if necessary.
  [profileMenu setHidden:YES];

  profileMenuController_.reset(
      [[ProfileMenuController alloc] initWithMainMenuItem:profileMenu]);
}

// The Confirm to Quit preference is atypical in that the preference lives in
// the app menu right above the Quit menu item. This method will refresh the
// display of that item depending on the preference state.
- (void)updateConfirmToQuitPrefMenuItem:(NSMenuItem*)item {
  // Format the string so that the correct key equivalent is displayed.
  NSString* acceleratorString = [ConfirmQuitPanelController keyCommandString];
  NSString* title = l10n_util::GetNSStringF(IDS_CONFIRM_TO_QUIT_OPTION,
      base::SysNSStringToUTF16(acceleratorString));
  [item setTitle:title];

  const PrefService* prefService = g_browser_process->local_state();
  bool enabled = prefService->GetBoolean(prefs::kConfirmToQuitEnabled);
  [item setState:enabled ? NSOnState : NSOffState];
}

- (void)updateDisplayMessageCenterPrefMenuItem:(NSMenuItem*)item {
  const PrefService* prefService = g_browser_process->local_state();
  bool enabled = prefService->GetBoolean(prefs::kMessageCenterShowIcon);
  // The item should be checked if "show icon" is false, since the text reads
  // "Hide notification center icon."
  [item setState:enabled ? NSOffState : NSOnState];
}

- (void)registerServicesMenuTypesTo:(NSApplication*)app {
  // Note that RenderWidgetHostViewCocoa implements NSServicesRequests which
  // handles requests from services.
  NSArray* types = [NSArray arrayWithObjects:NSStringPboardType, nil];
  [app registerServicesMenuSendTypes:types returnTypes:types];
}

- (Profile*)lastProfile {
  // Return the profile of the last-used BrowserWindowController, if available.
  if (lastProfile_)
    return lastProfile_;

  // On first launch, use the logic that ChromeBrowserMain uses to determine
  // the initial profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return NULL;

  return profile_manager->GetProfile(GetStartupProfilePath(
      profile_manager->user_data_dir(),
      *CommandLine::ForCurrentProcess()));
}

- (Profile*)safeLastProfileForNewWindows {
  Profile* profile = [self lastProfile];

  // Guest sessions must always be OffTheRecord. Use that when opening windows.
  if (profile->IsGuestSession())
    return profile->GetOffTheRecordProfile();

  return profile;
}

// Various methods to open URLs that we get in a native fashion. We use
// StartupBrowserCreator here because on the other platforms, URLs to open come
// through the ProcessSingleton, and it calls StartupBrowserCreator. It's best
// to bottleneck the openings through that for uniform handling.

- (void)openUrls:(const std::vector<GURL>&)urls {
  // If the browser hasn't started yet, just queue up the URLs.
  if (!startupComplete_) {
    startupUrls_.insert(startupUrls_.end(), urls.begin(), urls.end());
    return;
  }

  Browser* browser = chrome::GetLastActiveBrowser();
  // if no browser window exists then create one with no tabs to be filled in
  if (!browser) {
    browser = new Browser(Browser::CreateParams(
        [self lastProfile], chrome::HOST_DESKTOP_TYPE_NATIVE));
    browser->window()->Show();
  }

  CommandLine dummy(CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, first_run);
  launch.OpenURLsInBrowser(browser, false, urls, browser->host_desktop_type());
}

- (void)getUrl:(NSAppleEventDescriptor*)event
     withReply:(NSAppleEventDescriptor*)reply {
  NSString* urlStr = [[event paramDescriptorForKeyword:keyDirectObject]
                      stringValue];

  GURL gurl(base::SysNSStringToUTF8(urlStr));
  std::vector<GURL> gurlVector;
  gurlVector.push_back(gurl);

  [self openUrls:gurlVector];
}

- (void)application:(NSApplication*)sender
          openFiles:(NSArray*)filenames {
  std::vector<GURL> gurlVector;
  for (NSString* file in filenames) {
    GURL gurl =
        net::FilePathToFileURL(base::FilePath([file fileSystemRepresentation]));
    gurlVector.push_back(gurl);
  }
  if (!gurlVector.empty())
    [self openUrls:gurlVector];
  else
    NOTREACHED() << "Nothing to open!";

  [sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
}

// Show the preferences window, or bring it to the front if it's already
// visible.
- (IBAction)showPreferences:(id)sender {
  if (Browser* browser = ActivateBrowser([self lastProfile])) {
    // Show options tab in the active browser window.
    chrome::ShowSettings(browser);
  } else {
    // No browser window, so create one for the options tab.
    chrome::OpenOptionsWindow([self safeLastProfileForNewWindows]);
  }
}

- (IBAction)orderFrontStandardAboutPanel:(id)sender {
  if (Browser* browser = ActivateBrowser([self lastProfile])) {
    chrome::ShowAboutChrome(browser);
  } else {
    // No browser window, so create one for the about tab.
    chrome::OpenAboutWindow([self safeLastProfileForNewWindows]);
  }
}

- (IBAction)toggleConfirmToQuit:(id)sender {
  PrefService* prefService = g_browser_process->local_state();
  bool enabled = prefService->GetBoolean(prefs::kConfirmToQuitEnabled);
  prefService->SetBoolean(prefs::kConfirmToQuitEnabled, !enabled);
}

- (IBAction)toggleDisplayMessageCenter:(id)sender {
  PrefService* prefService = g_browser_process->local_state();
  bool enabled = prefService->GetBoolean(prefs::kMessageCenterShowIcon);
  prefService->SetBoolean(prefs::kMessageCenterShowIcon, !enabled);
}

// Explicitly bring to the foreground when creating new windows from the dock.
- (void)commandFromDock:(id)sender {
  [NSApp activateIgnoringOtherApps:YES];
  [self commandDispatch:sender];
}

- (NSMenu*)applicationDockMenu:(NSApplication*)sender {
  NSMenu* dockMenu = [[[NSMenu alloc] initWithTitle: @""] autorelease];
  Profile* profile = [self lastProfile];

  BOOL profilesAdded = [profileMenuController_ insertItemsIntoMenu:dockMenu
                                                          atOffset:0
                                                          fromDock:YES];
  if (profilesAdded)
    [dockMenu addItem:[NSMenuItem separatorItem]];

  NSString* titleStr = l10n_util::GetNSStringWithFixup(IDS_NEW_WINDOW_MAC);
  base::scoped_nsobject<NSMenuItem> item(
      [[NSMenuItem alloc] initWithTitle:titleStr
                                 action:@selector(commandFromDock:)
                          keyEquivalent:@""]);
  [item setTarget:self];
  [item setTag:IDC_NEW_WINDOW];
  [item setEnabled:[self validateUserInterfaceItem:item]];
  [dockMenu addItem:item];

  // |profile| can be NULL during unit tests.
  if (!profile || !profile->IsSupervised()) {
    titleStr = l10n_util::GetNSStringWithFixup(IDS_NEW_INCOGNITO_WINDOW_MAC);
    item.reset(
        [[NSMenuItem alloc] initWithTitle:titleStr
                                   action:@selector(commandFromDock:)
                            keyEquivalent:@""]);
    [item setTarget:self];
    [item setTag:IDC_NEW_INCOGNITO_WINDOW];
    [item setEnabled:[self validateUserInterfaceItem:item]];
    [dockMenu addItem:item];
  }

  // TODO(rickcam): Mock out BackgroundApplicationListModel, then add unit
  // tests which use the mock in place of the profile-initialized model.

  // Avoid breaking unit tests which have no profile.
  if (profile) {
    BackgroundApplicationListModel applications(profile);
    if (applications.size()) {
      int position = 0;
      NSString* menuStr =
          l10n_util::GetNSStringWithFixup(IDS_BACKGROUND_APPS_MAC);
      base::scoped_nsobject<NSMenu> appMenu(
          [[NSMenu alloc] initWithTitle:menuStr]);
      for (extensions::ExtensionList::const_iterator cursor =
               applications.begin();
           cursor != applications.end();
           ++cursor, ++position) {
        DCHECK_EQ(applications.GetPosition(cursor->get()), position);
        NSString* itemStr =
            base::SysUTF16ToNSString(base::UTF8ToUTF16((*cursor)->name()));
        base::scoped_nsobject<NSMenuItem> appItem(
            [[NSMenuItem alloc] initWithTitle:itemStr
                                       action:@selector(executeApplication:)
                                keyEquivalent:@""]);
        [appItem setTarget:self];
        [appItem setTag:position];
        [appMenu addItem:appItem];
      }
    }
  }

  return dockMenu;
}

- (const std::vector<GURL>&)startupUrls {
  return startupUrls_;
}

- (BookmarkMenuBridge*)bookmarkMenuBridge {
  return bookmarkMenuBridge_.get();
}

- (void)addObserverForWorkAreaChange:(ui::WorkAreaWatcherObserver*)observer {
  workAreaChangeObservers_.AddObserver(observer);
}

- (void)removeObserverForWorkAreaChange:(ui::WorkAreaWatcherObserver*)observer {
  workAreaChangeObservers_.RemoveObserver(observer);
}

- (void)initAppShimMenuController {
  if (!appShimMenuController_)
    appShimMenuController_.reset([[AppShimMenuController alloc] init]);
}

- (void)applicationDidChangeScreenParameters:(NSNotification*)notification {
  // During this callback the working area is not always already updated. Defer.
  [self performSelector:@selector(delayedScreenParametersUpdate)
             withObject:nil
             afterDelay:0];
}

- (void)delayedScreenParametersUpdate {
  FOR_EACH_OBSERVER(ui::WorkAreaWatcherObserver, workAreaChangeObservers_,
      WorkAreaChanged());
}

- (BOOL)application:(NSApplication*)application
    willContinueUserActivityWithType:(NSString*)userActivityType {
  return [userActivityType isEqualToString:NSUserActivityTypeBrowsingWeb];
}

- (BOOL)application:(NSApplication*)application
    continueUserActivity:(NSUserActivity*)userActivity
      restorationHandler:(void (^)(NSArray*))restorationHandler {
  if (![userActivity.activityType
          isEqualToString:NSUserActivityTypeBrowsingWeb]) {
    return NO;
  }

  NSURL* url = userActivity.webPageURL;
  if (!url)
    return NO;

  GURL gurl(base::SysNSStringToUTF8([url absoluteString]));
  std::vector<GURL> gurlVector;
  gurlVector.push_back(gurl);

  [self openUrls:gurlVector];
  return YES;
}

- (void)application:(NSApplication*)application
    didFailToContinueUserActivityWithType:(NSString*)userActivityType
                                    error:(NSError*)error {
}

@end  // @implementation AppController

//---------------------------------------------------------------------------

namespace app_controller_mac {

bool IsOpeningNewWindow() {
  return g_is_opening_new_window;
}

}  // namespace app_controller_mac
