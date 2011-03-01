// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/app_controller_mac.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/mac/mac_util.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/background_application_list_model.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/sync/sync_ui_util_mac.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_init.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/about_window_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#import "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/bug_report_window_controller.h"
#import "chrome/browser/ui/cocoa/clear_browsing_data_controller.h"
#import "chrome/browser/ui/cocoa/confirm_quit_panel_controller.h"
#import "chrome/browser/ui/cocoa/encoding_menu_controller_delegate_mac.h"
#import "chrome/browser/ui/cocoa/history_menu_bridge.h"
#import "chrome/browser/ui/cocoa/importer/import_settings_dialog.h"
#import "chrome/browser/ui/cocoa/options/preferences_window_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_window_controller.h"
#include "chrome/browser/ui/cocoa/task_manager_mac.h"
#include "chrome/browser/ui/options/options_window.h"
#include "chrome/common/app_mode_common_mac.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

// 10.6 adds a public API for the Spotlight-backed search menu item in the Help
// menu.  Provide the declaration so it can be called below when building with
// the 10.5 SDK.
#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
@interface NSApplication (SnowLeopardSDKDeclarations)
- (void)setHelpMenu:(NSMenu*)helpMenu;
@end
#endif

namespace {

// True while AppController is calling Browser::OpenEmptyWindow(). We need a
// global flag here, analogue to BrowserInit::InProcessStartup() because
// otherwise the SessionService will try to restore sessions when we make a new
// window while there are no other active windows.
bool g_is_opening_new_window = false;

// Activates a browser window having the given profile (the last one active) if
// possible and returns a pointer to the activate |Browser| or NULL if this was
// not possible. If the last active browser is minimized (in particular, if
// there are only minimized windows), it will unminimize it.
Browser* ActivateBrowser(Profile* profile) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  if (browser)
    browser->window()->Activate();
  return browser;
}

// Creates an empty browser window with the given profile and returns a pointer
// to the new |Browser|.
Browser* CreateBrowser(Profile* profile) {
  {
    AutoReset<bool> auto_reset_in_run(&g_is_opening_new_window, true);
    Browser::OpenEmptyWindow(profile);
  }

  Browser* browser = BrowserList::GetLastActive();
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

// This task synchronizes preferences (under "org.chromium.Chromium" or
// "com.google.Chrome"), in particular, writes them out to disk.
class PrefsSyncTask : public Task {
 public:
  PrefsSyncTask() {}
  virtual ~PrefsSyncTask() {}
  virtual void Run() {
    if (!CFPreferencesAppSynchronize(app_mode::kAppPrefsID))
      LOG(WARNING) << "Error recording application bundle path.";
  }
};

// Record the location of the application bundle (containing the main framework)
// from which Chromium was loaded. This is used by app mode shims to find
// Chromium.
void RecordLastRunAppBundlePath() {
  // Going up three levels from |chrome::GetVersionedDirectory()| gives the
  // real, user-visible app bundle directory. (The alternatives give either the
  // framework's path or the initial app's path, which may be an app mode shim
  // or a unit test.)
  FilePath appBundlePath =
      chrome::GetVersionedDirectory().DirName().DirName().DirName();
  CFPreferencesSetAppValue(app_mode::kLastRunAppBundlePathPrefsKey,
                           base::SysUTF8ToCFStringRef(appBundlePath.value()),
                           app_mode::kAppPrefsID);

  // Sync after a delay avoid I/O contention on startup; 1500 ms is plenty.
  BrowserThread::PostDelayedTask(BrowserThread::FILE, FROM_HERE,
                                 new PrefsSyncTask(), 1500);
}

}  // anonymous namespace

@interface AppController(Private)
- (void)initMenuState;
- (void)registerServicesMenuTypesTo:(NSApplication*)app;
- (void)openUrls:(const std::vector<GURL>&)urls;
- (void)getUrl:(NSAppleEventDescriptor*)event
     withReply:(NSAppleEventDescriptor*)reply;
- (void)windowLayeringDidChange:(NSNotification*)inNotification;
- (void)checkForAnyKeyWindows;
- (BOOL)userWillWaitForInProgressDownloads:(int)downloadCount;
- (BOOL)shouldQuitWithInProgressDownloads;
- (void)showPreferencesWindow:(id)sender
                         page:(OptionsPage)page
                      profile:(Profile*)profile;
- (void)executeApplication:(id)sender;
@end

@implementation AppController

@synthesize startupComplete = startupComplete_;

// This method is called very early in application startup (ie, before
// the profile is loaded or any preferences have been registered). Defer any
// user-data initialization until -applicationDidFinishLaunching:.
- (void)awakeFromNib {
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

  // Register for a notification that the number of tabs changes in windows
  // so we can adjust the close tab/window command keys.
  [notificationCenter
      addObserver:self
         selector:@selector(tabsChanged:)
             name:kTabStripNumberOfTabsChanged
           object:nil];

  // Set up the command updater for when there are no windows open
  [self initMenuState];

  // Activate (bring to foreground) if asked to do so.  On
  // Windows this logic isn't necessary since
  // BrowserWindow::Activate() calls ::SetForegroundWindow() which is
  // adequate.  On Mac, BrowserWindow::Activate() calls -[NSWindow
  // makeKeyAndOrderFront:] which does not activate the application
  // itself.
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(switches::kActivateOnLaunch)) {
    [NSApp activateIgnoringOtherApps:YES];
  }
}

// (NSApplicationDelegate protocol) This is the Apple-approved place to override
// the default handlers.
- (void)applicationWillFinishLaunching:(NSNotification*)notification {
  // Nothing here right now.
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

  size_t num_browsers = BrowserList::size();

  // Give any print jobs in progress time to finish.
  if (!browser_shutdown::IsTryingToQuit())
    g_browser_process->print_job_manager()->StopJobs(true);

  // Initiate a shutdown (via BrowserList::CloseAllBrowsers()) if we aren't
  // already shutting down.
  if (!browser_shutdown::IsTryingToQuit())
    BrowserList::CloseAllBrowsers();

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
  // Check if the experiment is enabled.
  const CommandLine* commandLine(CommandLine::ForCurrentProcess());
  if (commandLine->HasSwitch(switches::kDisableConfirmToQuit))
    return NSTerminateNow;

  // If the application is going to terminate as the result of a Cmd+Q
  // invocation, use the special sauce to prevent accidental quitting.
  // http://dev.chromium.org/developers/design-documents/confirm-to-quit-experiment

  // How long the user must hold down Cmd+Q to confirm the quit.
  const NSTimeInterval kTimeToConfirmQuit = 1.5;
  // Leeway between the |targetDate| and the current time that will confirm a
  // quit.
  const NSTimeInterval kTimeDeltaFuzzFactor = 1.0;
  // Duration of the window fade out animation.
  const NSTimeInterval kWindowFadeAnimationDuration = 0.2;

  // This logic is only for keyboard-initiated quits.
  if ([[app currentEvent] type] != NSKeyDown)
    return NSTerminateNow;

  // If this is the second of two such attempts to quit within a certain time
  // interval, then just quit.
  // Time of last quit attempt, if any.
  static NSDate* lastQuitAttempt; // Initially nil, as it's static.
  NSDate* timeNow = [NSDate date];
  if (lastQuitAttempt &&
      [timeNow timeIntervalSinceDate:lastQuitAttempt] < kTimeDeltaFuzzFactor) {
    return NSTerminateNow;
  } else {
    [lastQuitAttempt release]; // Harmless if already nil.
    lastQuitAttempt = [timeNow retain]; // Record this attempt for next time.
  }

  // Show the info panel that explains what the user must to do confirm quit.
  [[ConfirmQuitPanelController sharedController] showWindow:self];

  // Spin a nested run loop until the |targetDate| is reached or a KeyUp event
  // is sent.
  NSDate* targetDate =
      [NSDate dateWithTimeIntervalSinceNow:kTimeToConfirmQuit];
  BOOL willQuit = NO;
  NSEvent* nextEvent = nil;
  do {
    // Dequeue events until a key up is received.
    nextEvent = [app nextEventMatchingMask:NSKeyUpMask
                                 untilDate:nil
                                    inMode:NSEventTrackingRunLoopMode
                                   dequeue:YES];

    // Wait for the time expiry to happen. Once past the hold threshold,
    // commit to quitting and hide all the open windows.
    if (!willQuit) {
      NSDate* now = [NSDate date];
      NSTimeInterval difference = [targetDate timeIntervalSinceDate:now];
      if (difference < kTimeDeltaFuzzFactor) {
        willQuit = YES;

        // At this point, the quit has been confirmed and windows should all
        // fade out to convince the user to release the key combo to finalize
        // the quit.
        [NSAnimationContext beginGrouping];
        [[NSAnimationContext currentContext] setDuration:
            kWindowFadeAnimationDuration];
        for (NSWindow* aWindow in [app windows]) {
          // Windows that are set to animate and have a delegate do not
          // expect to be animated by other things and could result in an
          // invalid state. If a window is set up like so, just force the
          // alpha value to 0. Otherwise, animate all pretty and stuff.
          if (![[aWindow animationForKey:@"alphaValue"] delegate]) {
            [[aWindow animator] setAlphaValue:0.0];
          } else {
            [aWindow setAlphaValue:0.0];
          }
        }
        [NSAnimationContext endGrouping];
      }
    }
  } while (!nextEvent);

  // The user has released the key combo. Discard any events (i.e. the
  // repeated KeyDown Cmd+Q).
  [app discardEventsMatchingMask:NSAnyEventMask beforeEvent:nextEvent];

  if (willQuit) {
    // The user held down the combination long enough that quitting should
    // happen.
    return NSTerminateNow;
  } else {
    // Slowly fade the confirm window out in case the user doesn't
    // understand what they have to do to quit.
    [[ConfirmQuitPanelController sharedController] dismissPanel];
    return NSTerminateCancel;
  }

  // Default case: terminate.
  return NSTerminateNow;
}

// Called when the app is shutting down. Clean-up as appropriate.
- (void)applicationWillTerminate:(NSNotification*)aNotification {
  NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
  [em removeEventHandlerForEventClass:kInternetEventClass
                           andEventID:kAEGetURL];
  [em removeEventHandlerForEventClass:'WWW!'
                           andEventID:'OURL'];

  // There better be no browser windows left at this point.
  CHECK_EQ(BrowserList::size(), 0u);

  // Tell BrowserList not to keep the browser process alive. Once all the
  // browsers get dealloc'd, it will stop the RunLoop and fall back into main().
  BrowserList::EndKeepAlive();

  // Close these off if they have open windows.
  [prefsController_ close];
  [aboutController_ close];

  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)didEndMainMessageLoop {
  DCHECK(!BrowserList::HasBrowserWithProfile([self defaultProfile]));
  if (!BrowserList::HasBrowserWithProfile([self defaultProfile])) {
    // As we're shutting down, we need to nuke the TabRestoreService, which
    // will start the shutdown of the NavigationControllers and allow for
    // proper shutdown. If we don't do this, Chrome won't shut down cleanly,
    // and may end up crashing when some thread tries to use the IO thread (or
    // another thread) that is no longer valid.
    [self defaultProfile]->ResetTabRestoreService();
  }
}

// Helper routine to get the window controller if the key window is a tabbed
// window, or nil if not. Examples of non-tabbed windows are "about" or
// "preferences".
- (TabWindowController*)keyWindowTabController {
  NSWindowController* keyWindowController =
      [[NSApp keyWindow] windowController];
  if ([keyWindowController isKindOfClass:[TabWindowController class]])
    return (TabWindowController*)keyWindowController;

  return nil;
}

// Helper routine to get the window controller if the main window is a tabbed
// window, or nil if not. Examples of non-tabbed windows are "about" or
// "preferences".
- (TabWindowController*)mainWindowTabController {
  NSWindowController* mainWindowController =
      [[NSApp mainWindow] windowController];
  if ([mainWindowController isKindOfClass:[TabWindowController class]])
    return (TabWindowController*)mainWindowController;

  return nil;
}

// If the window has tabs, make "close window" be cmd-shift-w, otherwise leave
// it as the normal cmd-w. Capitalization of the key equivalent affects whether
// the shift modifer is used.
- (void)adjustCloseWindowMenuItemKeyEquivalent:(BOOL)inHaveTabs {
  [closeWindowMenuItem_ setKeyEquivalent:(inHaveTabs ? @"W" : @"w")];
  [closeWindowMenuItem_ setKeyEquivalentModifierMask:NSCommandKeyMask];
}

// If the window has tabs, make "close tab" take over cmd-w, otherwise it
// shouldn't have any key-equivalent because it should be disabled.
- (void)adjustCloseTabMenuItemKeyEquivalent:(BOOL)hasTabs {
  if (hasTabs) {
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

// See if we have a window with tabs open, and adjust the key equivalents for
// Close Tab/Close Window accordingly.
- (void)fixCloseMenuItemKeyEquivalents {
  fileMenuUpdatePending_ = NO;
  TabWindowController* tabController = [self keyWindowTabController];
  if (!tabController && ![NSApp keyWindow]) {
    // There might be a small amount of time where there is no key window,
    // so just use our main browser window if there is one.
    tabController = [self mainWindowTabController];
  }
  BOOL windowWithMultipleTabs =
      (tabController && [tabController numberOfTabs] > 1);
  [self adjustCloseWindowMenuItemKeyEquivalent:windowWithMultipleTabs];
  [self adjustCloseTabMenuItemKeyEquivalent:windowWithMultipleTabs];
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
}

- (void)checkForAnyKeyWindows {
  if ([NSApp keyWindow])
    return;

  NotificationService::current()->Notify(
      NotificationType::NO_KEY_WINDOW,
      NotificationService::AllSources(),
      NotificationService::NoDetails());
}

// Called when the number of tabs changes in one of the browser windows. The
// object is the tab strip controller, but we don't currently care.
- (void)tabsChanged:(NSNotification*)notify {
  // We don't need to do this on a delay, as in the method above, because the
  // window layering isn't changing. As a result, there's no chance that a
  // different window will sneak in as the key window and cause the problems
  // we hacked around above by clearing the key equivalents.
  [self fixCloseMenuItemKeyEquivalents];
}

// If the auto-update interval is not set, make it 5 hours.
// This code is specific to Mac Chrome Dev Channel.
// Placed here for 2 reasons:
// 1) Same spot as other Pref stuff
// 2) Try and be friendly by keeping this after app launch
// TODO(jrg): remove once we go Beta.
- (void)setUpdateCheckInterval {
#if defined(GOOGLE_CHROME_BUILD)
  CFStringRef app = (CFStringRef)@"com.google.Keystone.Agent";
  CFStringRef checkInterval = (CFStringRef)@"checkInterval";
  CFPropertyListRef plist = CFPreferencesCopyAppValue(checkInterval, app);
  if (!plist) {
    const float fiveHoursInSeconds = 5.0 * 60.0 * 60.0;
    NSNumber* value = [NSNumber numberWithFloat:fiveHoursInSeconds];
    CFPreferencesSetAppValue(checkInterval, value, app);
    CFPreferencesAppSynchronize(app);
  }
#endif
}

// This is called after profiles have been loaded and preferences registered.
// It is safe to access the default profile here.
- (void)applicationDidFinishLaunching:(NSNotification*)notify {
  // Notify BrowserList to keep the application running so it doesn't go away
  // when all the browser windows get closed.
  BrowserList::StartKeepAlive();

  bookmarkMenuBridge_.reset(new BookmarkMenuBridge([self defaultProfile]));
  historyMenuBridge_.reset(new HistoryMenuBridge([self defaultProfile]));

  [self setUpdateCheckInterval];

  // Build up the encoding menu, the order of the items differs based on the
  // current locale (see http://crbug.com/7647 for details).
  // We need a valid g_browser_process to get the profile which is why we can't
  // call this from awakeFromNib.
  NSMenu* view_menu = [[[NSApp mainMenu] itemWithTag:IDC_VIEW_MENU] submenu];
  NSMenuItem* encoding_menu_item = [view_menu itemWithTag:IDC_ENCODING_MENU];
  NSMenu* encoding_menu = [encoding_menu_item submenu];
  EncodingMenuControllerDelegate::BuildEncodingMenu([self defaultProfile],
                                                    encoding_menu);

  // Since Chrome is localized to more languages than the OS, tell Cocoa which
  // menu is the Help so it can add the search item to it.
  if (helpMenu_ && [NSApp respondsToSelector:@selector(setHelpMenu:)])
    [NSApp setHelpMenu:helpMenu_];

  // Record the path to the (browser) app bundle; this is used by the app mode
  // shim.
  RecordLastRunAppBundlePath();

  // Makes "Services" menu items available.
  [self registerServicesMenuTypesTo:[notify object]];

  startupComplete_ = YES;

  // TODO(viettrungluu): This is very temporary, since this should be done "in"
  // |BrowserMain()|, i.e., this list of startup URLs should be appended to the
  // (probably-empty) list of URLs from the command line.
  if (startupUrls_.size()) {
    [self openUrls:startupUrls_];
    [self clearStartupUrls];
  }
}

// This is called after profiles have been loaded and preferences registered.
// It is safe to access the default profile here.
- (void)applicationDidBecomeActive:(NSNotification*)notify {
  NotificationService::current()->Notify(NotificationType::APP_ACTIVATED,
                                         NotificationService::AllSources(),
                                         NotificationService::NoDetails());
}

// Helper function for populating and displaying the in progress downloads at
// exit alert panel.
- (BOOL)userWillWaitForInProgressDownloads:(int)downloadCount {
  NSString* warningText = nil;
  NSString* explanationText = nil;
  NSString* waitTitle = nil;
  NSString* exitTitle = nil;

  string16 product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);

  // Set the dialog text based on whether or not there are multiple downloads.
  if (downloadCount == 1) {
    // Dialog text: warning and explanation.
    warningText = l10n_util::GetNSStringF(
        IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_WARNING, product_name);
    explanationText = l10n_util::GetNSStringF(
        IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_EXPLANATION, product_name);

    // Cancel download and exit button text.
    exitTitle = l10n_util::GetNSString(
        IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_OK_BUTTON_LABEL);

    // Wait for download button text.
    waitTitle = l10n_util::GetNSString(
        IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_CANCEL_BUTTON_LABEL);
  } else {
    // Dialog text: warning and explanation.
    warningText = l10n_util::GetNSStringF(
        IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_WARNING, product_name,
        base::IntToString16(downloadCount));
    explanationText = l10n_util::GetNSStringF(
        IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_EXPLANATION, product_name);

    // Cancel downloads and exit button text.
    exitTitle = l10n_util::GetNSString(
        IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_OK_BUTTON_LABEL);

    // Wait for downloads button text.
    waitTitle = l10n_util::GetNSString(
        IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_CANCEL_BUTTON_LABEL);
  }

  // 'waitButton' is the default choice.
  int choice = NSRunAlertPanel(warningText, explanationText,
                               waitTitle, exitTitle, nil);
  return choice == NSAlertDefaultReturn ? YES : NO;
}

// Check all profiles for in progress downloads, and if we find any, prompt the
// user to see if we should continue to exit (and thus cancel the downloads), or
// if we should wait.
- (BOOL)shouldQuitWithInProgressDownloads {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return YES;

  ProfileManager::const_iterator it = profile_manager->begin();
  for (; it != profile_manager->end(); ++it) {
    Profile* profile = *it;
    DownloadManager* download_manager = profile->GetDownloadManager();
    if (download_manager && download_manager->in_progress_count() > 0) {
      int downloadCount = download_manager->in_progress_count();
      if ([self userWillWaitForInProgressDownloads:downloadCount]) {
        // Create a new browser window (if necessary) and navigate to the
        // downloads page if the user chooses to wait.
        Browser* browser = BrowserList::FindBrowserWithProfile(profile);
        if (!browser) {
          browser = Browser::Create(profile);
          browser->window()->Show();
        }
        DCHECK(browser);
        browser->ShowDownloadsTab();
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
  TabRestoreService* service = [self defaultProfile]->GetTabRestoreService();
  return service && !service->entries().empty();
}

// Returns true if there is not a modal window (either window- or application-
// modal) blocking the active browser. Note that tab modal dialogs (HTTP auth
// sheets) will not count as blocking the browser. But things like open/save
// dialogs that are window modal will block the browser.
- (BOOL)keyWindowIsNotModal {
  Browser* browser = BrowserList::GetLastActive();
  return [NSApp modalWindow] == nil && (!browser ||
         ![[browser->window()->GetNativeHandle() attachedSheet]
             isKindOfClass:[NSWindow class]]);
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
  if (action == @selector(commandDispatch:)) {
    NSInteger tag = [item tag];
    if (menuState_->SupportsCommand(tag)) {
      switch (tag) {
        // The File Menu commands are not automatically disabled by Cocoa when a
        // dialog sheet obscures the browser window, so we disable several of
        // them here.  We don't need to include IDC_CLOSE_WINDOW, because
        // app_controller is only activated when there are no key windows (see
        // function comment).
        case IDC_RESTORE_TAB:
          enable = [self keyWindowIsNotModal] && [self canRestoreTab];
          break;
        // Browser-level items that open in new tabs should not open if there's
        // a window- or app-modal dialog.
        case IDC_OPEN_FILE:
        case IDC_NEW_TAB:
        case IDC_SHOW_HISTORY:
        case IDC_SHOW_BOOKMARK_MANAGER:
          enable = [self keyWindowIsNotModal];
          break;
        // Browser-level items that open in new windows.
        case IDC_NEW_WINDOW:
        case IDC_TASK_MANAGER:
          // Allow the user to open a new window if there's a window-modal
          // dialog.
          enable = [self keyWindowIsNotModal] || ([NSApp modalWindow] == nil);
          break;
        case IDC_SYNC_BOOKMARKS: {
          Profile* defaultProfile = [self defaultProfile];
          // The profile may be NULL during shutdown -- see
          // http://code.google.com/p/chromium/issues/detail?id=43048 .
          //
          // TODO(akalin,viettrungluu): Figure out whether this method
          // can be prevented from being called if defaultProfile is
          // NULL.
          if (!defaultProfile) {
            LOG(WARNING)
                << "NULL defaultProfile detected -- not doing anything";
            break;
          }
          enable = defaultProfile->IsSyncAccessible() &&
              [self keyWindowIsNotModal];
          sync_ui_util::UpdateSyncItem(item, enable, defaultProfile);
          break;
        }
        default:
          enable = menuState_->IsCommandEnabled(tag) ?
                   [self keyWindowIsNotModal] : NO;
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
  }
  return enable;
}

// Called when the user picks a menu item when there are no key windows, or when
// there is no foreground browser window. Calls through to the browser object to
// execute the command. This assumes that the command is supported and doesn't
// check, otherwise it should have been disabled in the UI in
// |-validateUserInterfaceItem:|.
- (void)commandDispatch:(id)sender {
  Profile* defaultProfile = [self defaultProfile];

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

  NSInteger tag = [sender tag];
  switch (tag) {
    case IDC_NEW_TAB:
      // Create a new tab in an existing browser window (which we activate) if
      // possible.
      if (Browser* browser = ActivateBrowser(defaultProfile)) {
        browser->ExecuteCommand(IDC_NEW_TAB);
        break;
      }
      // Else fall through to create new window.
    case IDC_NEW_WINDOW:
      CreateBrowser(defaultProfile);
      break;
    case IDC_FOCUS_LOCATION:
      ActivateOrCreateBrowser(defaultProfile)->
          ExecuteCommand(IDC_FOCUS_LOCATION);
      break;
    case IDC_FOCUS_SEARCH:
      ActivateOrCreateBrowser(defaultProfile)->ExecuteCommand(IDC_FOCUS_SEARCH);
      break;
    case IDC_NEW_INCOGNITO_WINDOW:
      Browser::OpenEmptyWindow(defaultProfile->GetOffTheRecordProfile());
      break;
    case IDC_RESTORE_TAB:
      Browser::OpenWindowWithRestoredTabs(defaultProfile);
      break;
    case IDC_OPEN_FILE:
      CreateBrowser(defaultProfile)->ExecuteCommand(IDC_OPEN_FILE);
      break;
    case IDC_CLEAR_BROWSING_DATA: {
      // There may not be a browser open, so use the default profile.
      if (CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableTabbedOptions)) {
        [ClearBrowsingDataController
            showClearBrowsingDialogForProfile:defaultProfile];
      } else {
        if (Browser* browser = ActivateBrowser(defaultProfile)) {
          browser->OpenClearBrowsingDataDialog();
        } else {
          Browser::OpenClearBrowingDataDialogWindow(defaultProfile);
        }
      }
      break;
    }
    case IDC_IMPORT_SETTINGS: {
      if (CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableTabbedOptions)) {
        UserMetrics::RecordAction(UserMetricsAction("Import_ShowDlg"),
                                  defaultProfile);
        [ImportSettingsDialogController
            showImportSettingsDialogForProfile:defaultProfile];
      } else {
        if (Browser* browser = ActivateBrowser(defaultProfile)) {
          browser->OpenImportSettingsDialog();
        } else {
          Browser::OpenImportSettingsDialogWindow(defaultProfile);
        }
      }
      break;
    }
    case IDC_SHOW_BOOKMARK_MANAGER:
      UserMetrics::RecordAction(UserMetricsAction("ShowBookmarkManager"),
                                defaultProfile);
      if (Browser* browser = ActivateBrowser(defaultProfile)) {
        // Open a bookmark manager tab.
        browser->OpenBookmarkManager();
      } else {
        // No browser window, so create one for the bookmark manager tab.
        Browser::OpenBookmarkManagerWindow(defaultProfile);
      }
      break;
    case IDC_SHOW_HISTORY:
      if (Browser* browser = ActivateBrowser(defaultProfile))
        browser->ShowHistoryTab();
      else
        Browser::OpenHistoryWindow(defaultProfile);
      break;
    case IDC_SHOW_DOWNLOADS:
      if (Browser* browser = ActivateBrowser(defaultProfile))
        browser->ShowDownloadsTab();
      else
        Browser::OpenDownloadsWindow(defaultProfile);
      break;
    case IDC_MANAGE_EXTENSIONS:
      if (Browser* browser = ActivateBrowser(defaultProfile))
        browser->ShowExtensionsTab();
      else
        Browser::OpenExtensionsWindow(defaultProfile);
      break;
    case IDC_HELP_PAGE:
      if (Browser* browser = ActivateBrowser(defaultProfile))
        browser->OpenHelpTab();
      else
        Browser::OpenHelpWindow(defaultProfile);
      break;
    case IDC_FEEDBACK: {
      Browser* browser = BrowserList::GetLastActive();
      TabContents* currentTab =
          browser ? browser->GetSelectedTabContents() : NULL;
      BugReportWindowController* controller =
          [[BugReportWindowController alloc]
              initWithTabContents:currentTab
                          profile:[self defaultProfile]];
      [controller runModalDialog];
      break;
    }
    case IDC_SYNC_BOOKMARKS:
      // The profile may be NULL during shutdown -- see
      // http://code.google.com/p/chromium/issues/detail?id=43048 .
      //
      // TODO(akalin,viettrungluu): Figure out whether this method can
      // be prevented from being called if defaultProfile is NULL.
      if (!defaultProfile) {
        LOG(WARNING) << "NULL defaultProfile detected -- not doing anything";
        break;
      }
      // TODO(akalin): Add a constant to denote starting sync from the
      // main menu and use that instead of START_FROM_WRENCH.
      sync_ui_util::OpenSyncMyBookmarksDialog(
          defaultProfile, ActivateBrowser(defaultProfile),
          ProfileSyncService::START_FROM_WRENCH);
      break;
    case IDC_TASK_MANAGER:
      UserMetrics::RecordAction(UserMetricsAction("TaskManager"),
                                defaultProfile);
      TaskManagerMac::Show(false);
      break;
    case IDC_OPTIONS:
      [self showPreferences:sender];
      break;
    default:
      // Background Applications use dynamic values that must be less than the
      // smallest value among the predefined IDC_* labels.
      if ([sender tag] < IDC_MinimumLabelValue)
        [self executeApplication:sender];
      break;
  }
}

// Run a (background) application in a new tab.
- (void)executeApplication:(id)sender {
  NSInteger tag = [sender tag];
  Profile* profile = [self defaultProfile];
  DCHECK(profile);
  BackgroundApplicationListModel applications(profile);
  DCHECK(tag >= 0 &&
         tag < static_cast<int>(applications.size()));
  Browser* browser = BrowserList::GetLastActive();
  if (!browser) {
    Browser::OpenEmptyWindow(profile);
    browser = BrowserList::GetLastActive();
  }
  const Extension* extension = applications.GetExtension(tag);
  browser->OpenApplicationTab(profile, extension, NULL);
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

// NSApplication delegate method called when someone clicks on the
// dock icon and there are no open windows.  To match standard mac
// behavior, we should open a new window.
- (BOOL)applicationShouldHandleReopen:(NSApplication*)theApplication
                    hasVisibleWindows:(BOOL)flag {
  // If the browser is currently trying to quit, don't do anything and return NO
  // to prevent AppKit from doing anything.
  // TODO(rohitrao): Remove this code when http://crbug.com/40861 is resolved.
  if (browser_shutdown::IsTryingToQuit())
    return NO;

  // Don't do anything if there are visible windows.  This will cause
  // AppKit to unminimize the most recently minimized window.
  if (flag)
    return YES;

  // If launched as a hidden login item (due to installation of a persistent app
  // or by the user, for example in System Preferenecs->Accounts->Login Items),
  // allow session to be restored first time the user clicks on a Dock icon.
  // Normally, it'd just open a new empty page.
  {
      static BOOL doneOnce = NO;
      if (!doneOnce) {
        doneOnce = YES;
        if (base::mac::WasLaunchedAsHiddenLoginItem()) {
          SessionService* sessionService =
              [self defaultProfile]->GetSessionService();
          if (sessionService &&
              sessionService->RestoreIfNecessary(std::vector<GURL>()))
            return NO;
        }
      }
  }
  // Otherwise open a new window.
  {
    AutoReset<bool> auto_reset_in_run(&g_is_opening_new_window, true);
    Browser::OpenEmptyWindow([self defaultProfile]);
  }

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
  menuState_->UpdateCommandEnabled(IDC_HELP_PAGE, true);
  menuState_->UpdateCommandEnabled(IDC_IMPORT_SETTINGS, true);
  menuState_->UpdateCommandEnabled(IDC_FEEDBACK, true);
  menuState_->UpdateCommandEnabled(IDC_SYNC_BOOKMARKS, true);
  menuState_->UpdateCommandEnabled(IDC_TASK_MANAGER, true);
}

- (void)registerServicesMenuTypesTo:(NSApplication*)app {
  // Note that RenderWidgetHostViewCocoa implements NSServicesRequests which
  // handles requests from services.
  NSArray* types = [NSArray arrayWithObjects:NSStringPboardType, nil];
  [app registerServicesMenuSendTypes:types returnTypes:types];
}

- (Profile*)defaultProfile {
  // TODO(jrg): Find a better way to get the "default" profile.
  if (g_browser_process->profile_manager())
    return *g_browser_process->profile_manager()->begin();

  return NULL;
}

// Various methods to open URLs that we get in a native fashion. We use
// BrowserInit here because on the other platforms, URLs to open come through
// the ProcessSingleton, and it calls BrowserInit. It's best to bottleneck the
// openings through that for uniform handling.

- (void)openUrls:(const std::vector<GURL>&)urls {
  // If the browser hasn't started yet, just queue up the URLs.
  if (!startupComplete_) {
    startupUrls_.insert(startupUrls_.end(), urls.begin(), urls.end());
    return;
  }

  Browser* browser = BrowserList::GetLastActive();
  // if no browser window exists then create one with no tabs to be filled in
  if (!browser) {
    browser = Browser::Create([self defaultProfile]);
    browser->window()->Show();
  }

  CommandLine dummy(CommandLine::NO_PROGRAM);
  BrowserInit::LaunchWithProfile launch(FilePath(), dummy);
  launch.OpenURLsInBrowser(browser, false, urls);
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
    GURL gurl = net::FilePathToFileURL(FilePath(base::SysNSStringToUTF8(file)));
    gurlVector.push_back(gurl);
  }
  if (!gurlVector.empty())
    [self openUrls:gurlVector];
  else
    NOTREACHED() << "Nothing to open!";

  [sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
}

// Called when the preferences window is closed. We use this to release the
// window controller.
- (void)prefsWindowClosed:(NSNotification*)notification {
  NSWindow* window = [prefsController_ window];
  DCHECK_EQ([notification object], window);
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter removeObserver:self
                           name:NSWindowWillCloseNotification
                         object:window];
  // PreferencesWindowControllers are autoreleased in
  // -[PreferencesWindowController windowWillClose:].
  prefsController_ = nil;
}

// Show the preferences window, or bring it to the front if it's already
// visible.
- (IBAction)showPreferences:(id)sender {
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (!parsed_command_line.HasSwitch(switches::kDisableTabbedOptions)) {
    if (Browser* browser = ActivateBrowser([self defaultProfile])) {
      // Show options tab in the active browser window.
      browser->ShowOptionsTab(chrome::kDefaultOptionsSubPage);
    } else {
      // No browser window, so create one for the options tab.
      Browser::OpenOptionsWindow([self defaultProfile]);
    }
  } else {
    [self showPreferencesWindow:sender
                           page:OPTIONS_PAGE_DEFAULT
                        profile:[self defaultProfile]];
  }
}

- (void)showPreferencesWindow:(id)sender
                         page:(OptionsPage)page
                      profile:(Profile*)profile {
  if (prefsController_) {
    [prefsController_ switchToPage:page animate:YES];
  } else {
    prefsController_ =
        [[PreferencesWindowController alloc] initWithProfile:profile
                                                 initialPage:page];
    // Watch for a notification of when it goes away so that we can destroy
    // the controller.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(prefsWindowClosed:)
               name:NSWindowWillCloseNotification
             object:[prefsController_ window]];
  }
  [prefsController_ showPreferences:sender];
}

// Called when the about window is closed. We use this to release the
// window controller.
- (void)aboutWindowClosed:(NSNotification*)notification {
  NSWindow* window = [aboutController_ window];
  DCHECK_EQ([notification object], window);
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSWindowWillCloseNotification
              object:window];
  // AboutWindowControllers are autoreleased in
  // -[AboutWindowController windowWillClose:].
  aboutController_ = nil;
}

- (IBAction)orderFrontStandardAboutPanel:(id)sender {
  if (!aboutController_) {
    aboutController_ =
        [[AboutWindowController alloc] initWithProfile:[self defaultProfile]];

    // Watch for a notification of when it goes away so that we can destroy
    // the controller.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(aboutWindowClosed:)
               name:NSWindowWillCloseNotification
             object:[aboutController_ window]];
  }

  [aboutController_ showWindow:self];
}

// Explicitly bring to the foreground when creating new windows from the dock.
- (void)commandFromDock:(id)sender {
  [NSApp activateIgnoringOtherApps:YES];
  [self commandDispatch:sender];
}

- (NSMenu*)applicationDockMenu:(NSApplication*)sender {
  NSMenu* dockMenu = [[[NSMenu alloc] initWithTitle: @""] autorelease];
  Profile* profile = [self defaultProfile];

  NSString* titleStr = l10n_util::GetNSStringWithFixup(IDS_NEW_WINDOW_MAC);
  scoped_nsobject<NSMenuItem> item(
      [[NSMenuItem alloc] initWithTitle:titleStr
                                 action:@selector(commandFromDock:)
                          keyEquivalent:@""]);
  [item setTarget:self];
  [item setTag:IDC_NEW_WINDOW];
  [dockMenu addItem:item];

  titleStr = l10n_util::GetNSStringWithFixup(IDS_NEW_INCOGNITO_WINDOW_MAC);
  item.reset([[NSMenuItem alloc] initWithTitle:titleStr
                                        action:@selector(commandFromDock:)
                                 keyEquivalent:@""]);
  [item setTarget:self];
  [item setTag:IDC_NEW_INCOGNITO_WINDOW];
  [dockMenu addItem:item];

  // TODO(rickcam): Mock out BackgroundApplicationListModel, then add unit
  // tests which use the mock in place of the profile-initialized model.

  // Avoid breaking unit tests which have no profile.
  if (profile) {
    BackgroundApplicationListModel applications(profile);
    if (applications.size()) {
      int position = 0;
      NSString* menuStr =
          l10n_util::GetNSStringWithFixup(IDS_BACKGROUND_APPS_MAC);
      scoped_nsobject<NSMenu> appMenu([[NSMenu alloc] initWithTitle:menuStr]);
      for (ExtensionList::const_iterator cursor = applications.begin();
           cursor != applications.end();
           ++cursor, ++position) {
        DCHECK_EQ(applications.GetPosition(*cursor), position);
        NSString* itemStr =
            base::SysUTF16ToNSString(UTF8ToUTF16((*cursor)->name()));
        scoped_nsobject<NSMenuItem> appItem([[NSMenuItem alloc]
            initWithTitle:itemStr
                   action:@selector(commandFromDock:)
            keyEquivalent:@""]);
        [appItem setTarget:self];
        [appItem setTag:position];
        [appMenu addItem:appItem];
      }
      scoped_nsobject<NSMenuItem> appMenuItem([[NSMenuItem alloc]
          initWithTitle:menuStr
                 action:@selector(commandFromDock:)
          keyEquivalent:@""]);
      [appMenuItem setTarget:self];
      [appMenuItem setTag:position];
      [appMenuItem setSubmenu:appMenu];
      [dockMenu addItem:appMenuItem];
    }
  }

  return dockMenu;
}

- (const std::vector<GURL>&)startupUrls {
  return startupUrls_;
}

- (void)clearStartupUrls {
  startupUrls_.clear();
}

@end  // @implementation AppController

//---------------------------------------------------------------------------

void ShowOptionsWindow(OptionsPage page,
                       OptionsGroup highlight_group,
                       Profile* profile) {
  // TODO(akalin): Use highlight_group.
  AppController* appController = [NSApp delegate];
  [appController showPreferencesWindow:nil page:page profile:profile];
}

namespace app_controller_mac {

bool IsOpeningNewWindow() {
  return g_is_opening_new_window;
}

}  // namespace app_controller_mac
