// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/app_controller_mac.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/background/background_application_list_model.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/instant/instant_confirm_dialog.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/cloud_print/virtual_driver_install_helper.h"
#include "chrome/browser/printing/print_dialog_cloud.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/sync/sync_ui_util_mac.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_init.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/about_window_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#import "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/confirm_quit_panel_controller.h"
#import "chrome/browser/ui/cocoa/encoding_menu_controller_delegate_mac.h"
#import "chrome/browser/ui/cocoa/history_menu_bridge.h"
#import "chrome/browser/ui/cocoa/profile_menu_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_window_controller.h"
#include "chrome/browser/ui/cocoa/task_manager_mac.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_class_mac.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/service_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/accelerators/accelerator_cocoa.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

using content::BrowserThread;

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

CFStringRef BaseBundleID_CFString() {
  NSString* base_bundle_id =
      [NSString stringWithUTF8String:base::mac::BaseBundleID()];
  return base::mac::NSToCFCast(base_bundle_id);
}

// This task synchronizes preferences (under "org.chromium.Chromium" or
// "com.google.Chrome"), in particular, writes them out to disk.
class PrefsSyncTask : public Task {
 public:
  PrefsSyncTask() {}
  virtual ~PrefsSyncTask() {}
  virtual void Run() {
    if (!CFPreferencesAppSynchronize(BaseBundleID_CFString()))
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
                           BaseBundleID_CFString());

  // Sync after a delay avoid I/O contention on startup; 1500 ms is plenty.
  BrowserThread::PostDelayedTask(BrowserThread::FILE, FROM_HERE,
                                 new PrefsSyncTask(), 1500);
}

}  // anonymous namespace

const AEEventClass kAECloudPrintInstallClass = 'GCPi';
const AEEventClass kAECloudPrintUninstallClass = 'GCPu';

@interface AppController (Private)
- (void)initMenuState;
- (void)initProfileMenu;
- (void)updateConfirmToQuitPrefMenuItem:(NSMenuItem*)item;
- (void)registerServicesMenuTypesTo:(NSApplication*)app;
- (void)openUrls:(const std::vector<GURL>&)urls;
- (void)getUrl:(NSAppleEventDescriptor*)event
     withReply:(NSAppleEventDescriptor*)reply;
- (void)submitCloudPrintJob:(NSAppleEventDescriptor*)event;
- (void)installCloudPrint:(NSAppleEventDescriptor*)event;
- (void)uninstallCloudPrint:(NSAppleEventDescriptor*)event;
- (void)windowLayeringDidChange:(NSNotification*)inNotification;
- (void)windowChangedToProfile:(Profile*)profile;
- (void)checkForAnyKeyWindows;
- (BOOL)userWillWaitForInProgressDownloads:(int)downloadCount;
- (BOOL)shouldQuitWithInProgressDownloads;
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
          andSelector:@selector(submitCloudPrintJob:)
        forEventClass:cloud_print::kAECloudPrintClass
           andEventID:cloud_print::kAECloudPrintClass];
  // Install and uninstall handlers for virtual drivers.
  [em setEventHandler:self
          andSelector:@selector(installCloudPrint:)
        forEventClass:kAECloudPrintInstallClass
           andEventID:kAECloudPrintInstallClass];
  [em setEventHandler:self
          andSelector:@selector(uninstallCloudPrint:)
        forEventClass:kAECloudPrintUninstallClass
           andEventID:kAECloudPrintUninstallClass];
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

  // Set up the command updater for when there are no windows open
  [self initMenuState];

  // Initialize the Profile menu.
  [self initProfileMenu];
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
  // Check if the preference is turned on.
  const PrefService* prefs = [self lastProfile]->GetPrefs();
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
  [aboutController_ close];

  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)didEndMainMessageLoop {
  DCHECK(!BrowserList::HasBrowserWithProfile([self lastProfile]));
  if (!BrowserList::HasBrowserWithProfile([self lastProfile])) {
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
// affects whether the shift modifer is used.
- (void)adjustCloseWindowMenuItemKeyEquivalent:(BOOL)hasTabs {
  [closeWindowMenuItem_ setKeyEquivalent:(hasTabs ? @"W" : @"w")];
  [closeWindowMenuItem_ setKeyEquivalentModifierMask:NSCommandKeyMask];
}

// If the window has a tab controller, make "close tab" take over cmd-w,
// otherwise it shouldn't have any key-equivalent because it should be disabled.
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
  [self adjustCloseWindowMenuItemKeyEquivalent:hasTabs];
  [self adjustCloseTabMenuItemKeyEquivalent:hasTabs];
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
  if ([windowController isKindOfClass:[BrowserWindowController class]]) {
    // If the profile is incognito, use the original profile.
    Profile* newProfile = [windowController profile]->GetOriginalProfile();
    [self windowChangedToProfile:newProfile];
  } else if (BrowserList::empty()) {
    [self windowChangedToProfile:
        g_browser_process->profile_manager()->GetLastUsedProfile()];
  }
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
  bookmarkMenuBridge_->BuildMenu();

  historyMenuBridge_.reset(new HistoryMenuBridge(lastProfile_));
  historyMenuBridge_->BuildMenu();
}

- (void)checkForAnyKeyWindows {
  if ([NSApp keyWindow])
    return;

  content::NotificationService::current()->Notify(
      content::NOTIFICATION_NO_KEY_WINDOW,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
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

  [self setUpdateCheckInterval];

  // Build up the encoding menu, the order of the items differs based on the
  // current locale (see http://crbug.com/7647 for details).
  // We need a valid g_browser_process to get the profile which is why we can't
  // call this from awakeFromNib.
  NSMenu* viewMenu = [[[NSApp mainMenu] itemWithTag:IDC_VIEW_MENU] submenu];
  NSMenuItem* encodingMenuItem = [viewMenu itemWithTag:IDC_ENCODING_MENU];
  NSMenu* encodingMenu = [encodingMenuItem submenu];
  EncodingMenuControllerDelegate::BuildEncodingMenu([self lastProfile],
                                                    encodingMenu);

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

  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (!parsed_command_line.HasSwitch(switches::kEnableExposeForTabs)) {
    [tabposeMenuItem_ setHidden:YES];
  }
}

// This is called after profiles have been loaded and preferences registered.
// It is safe to access the default profile here.
- (void)applicationDidBecomeActive:(NSNotification*)notify {
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_APP_ACTIVATED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
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

  std::vector<Profile*> profiles(profile_manager->GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i) {
    DownloadService* download_service =
      DownloadServiceFactory::GetForProfile(profiles[i]);
    DownloadManager* download_manager =
        (download_service->HasCreatedDownloadManager() ?
         download_service->GetDownloadManager() : NULL);
    if (download_manager && download_manager->InProgressCount() > 0) {
      int downloadCount = download_manager->InProgressCount();
      if ([self userWillWaitForInProgressDownloads:downloadCount]) {
        // Create a new browser window (if necessary) and navigate to the
        // downloads page if the user chooses to wait.
        Browser* browser = BrowserList::FindBrowserWithProfile(profiles[i]);
        if (!browser) {
          browser = Browser::Create(profiles[i]);
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
  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile([self lastProfile]);
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
          enable = lastProfile->IsSyncAccessible() &&
              [self keyWindowIsNotModal];
          sync_ui_util::UpdateSyncItem(item, enable, lastProfile);
          break;
        }
        case IDC_FEEDBACK:
          enable = NO;
          break;
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
  } else if (action == @selector(toggleConfirmToQuit:)) {
    [self updateConfirmToQuitPrefMenuItem:static_cast<NSMenuItem*>(item)];
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
  Profile* lastProfile = [self lastProfile];

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
      if (Browser* browser = ActivateBrowser(lastProfile)) {
        browser->ExecuteCommand(IDC_NEW_TAB);
        break;
      }
      // Else fall through to create new window.
    case IDC_NEW_WINDOW:
      CreateBrowser(lastProfile);
      break;
    case IDC_FOCUS_LOCATION:
      ActivateOrCreateBrowser(lastProfile)->ExecuteCommand(IDC_FOCUS_LOCATION);
      break;
    case IDC_FOCUS_SEARCH:
      ActivateOrCreateBrowser(lastProfile)->ExecuteCommand(IDC_FOCUS_SEARCH);
      break;
    case IDC_NEW_INCOGNITO_WINDOW:
      Browser::OpenEmptyWindow(lastProfile->GetOffTheRecordProfile());
      break;
    case IDC_RESTORE_TAB:
      Browser::OpenWindowWithRestoredTabs(lastProfile);
      break;
    case IDC_OPEN_FILE:
      CreateBrowser(lastProfile)->ExecuteCommand(IDC_OPEN_FILE);
      break;
    case IDC_CLEAR_BROWSING_DATA: {
      // There may not be a browser open, so use the default profile.
      if (Browser* browser = ActivateBrowser(lastProfile)) {
        browser->OpenClearBrowsingDataDialog();
      } else {
        Browser::OpenClearBrowsingDataDialogWindow(lastProfile);
      }
      break;
    }
    case IDC_IMPORT_SETTINGS: {
      if (Browser* browser = ActivateBrowser(lastProfile)) {
        browser->OpenImportSettingsDialog();
      } else {
        Browser::OpenImportSettingsDialogWindow(lastProfile);
      }
      break;
    }
    case IDC_SHOW_BOOKMARK_MANAGER:
      UserMetrics::RecordAction(UserMetricsAction("ShowBookmarkManager"));
      if (Browser* browser = ActivateBrowser(lastProfile)) {
        // Open a bookmark manager tab.
        browser->OpenBookmarkManager();
      } else {
        // No browser window, so create one for the bookmark manager tab.
        Browser::OpenBookmarkManagerWindow(lastProfile);
      }
      break;
    case IDC_SHOW_HISTORY:
      if (Browser* browser = ActivateBrowser(lastProfile))
        browser->ShowHistoryTab();
      else
        Browser::OpenHistoryWindow(lastProfile);
      break;
    case IDC_SHOW_DOWNLOADS:
      if (Browser* browser = ActivateBrowser(lastProfile))
        browser->ShowDownloadsTab();
      else
        Browser::OpenDownloadsWindow(lastProfile);
      break;
    case IDC_MANAGE_EXTENSIONS:
      if (Browser* browser = ActivateBrowser(lastProfile))
        browser->ShowExtensionsTab();
      else
        Browser::OpenExtensionsWindow(lastProfile);
      break;
    case IDC_HELP_PAGE:
      if (Browser* browser = ActivateBrowser(lastProfile))
        browser->ShowHelpTab();
      else
        Browser::OpenHelpWindow(lastProfile);
      break;
    case IDC_SYNC_BOOKMARKS:
      // The profile may be NULL during shutdown -- see
      // http://code.google.com/p/chromium/issues/detail?id=43048 .
      //
      // TODO(akalin,viettrungluu): Figure out whether this method can
      // be prevented from being called if lastProfile is NULL.
      if (!lastProfile) {
        LOG(WARNING) << "NULL lastProfile detected -- not doing anything";
        break;
      }
      // TODO(akalin): Add a constant to denote starting sync from the
      // main menu and use that instead of START_FROM_WRENCH.
      sync_ui_util::OpenSyncMyBookmarksDialog(
          lastProfile, ActivateBrowser(lastProfile),
          ProfileSyncService::START_FROM_WRENCH);
      break;
    case IDC_TASK_MANAGER:
      UserMetrics::RecordAction(UserMetricsAction("TaskManager"));
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
  Profile* profile = [self lastProfile];
  DCHECK(profile);
  BackgroundApplicationListModel applications(profile);
  DCHECK(tag >= 0 &&
         tag < static_cast<int>(applications.size()));
  const Extension* extension = applications.GetExtension(tag);
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
              SessionServiceFactory::GetForProfile([self lastProfile]);
          if (sessionService &&
              sessionService->RestoreIfNecessary(std::vector<GURL>()))
            return NO;
        }
      }
  }
  // Otherwise open a new window.
  {
    AutoReset<bool> auto_reset_in_run(&g_is_opening_new_window, true);
    Browser::OpenEmptyWindow([self lastProfile]);
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

// Conditionally adds the Profile menu to the main menu bar.
- (void)initProfileMenu {
  NSMenu* mainMenu = [NSApp mainMenu];
  NSMenuItem* profileMenu = [mainMenu itemWithTag:IDC_PROFILE_MAIN_MENU];

  // On Leopard, hiding main menubar items does not work. This manifests itself
  // in Chromium as squished menu items <http://crbug.com/90753>. To prevent
  // this, remove the Profile menu on Leopard, regardless of the user's
  // multiprofile state.
  if (!ProfileManager::IsMultipleProfilesEnabled() ||
      base::mac::IsOSLeopard()) {
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

  const PrefService* prefService = [self lastProfile]->GetPrefs();
  bool enabled = prefService->GetBoolean(prefs::kConfirmToQuitEnabled);
  [item setState:enabled ? NSOnState : NSOffState];
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

  // On first launch, no profile will be stored, so use last from Local State.
  if (g_browser_process->profile_manager())
    return g_browser_process->profile_manager()->GetLastUsedProfile();

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
    browser = Browser::Create([self lastProfile]);
    browser->window()->Show();
  }

  CommandLine dummy(CommandLine::NO_PROGRAM);
  BrowserInit::IsFirstRun first_run = FirstRun::IsChromeFirstRun() ?
      BrowserInit::IS_FIRST_RUN : BrowserInit::IS_NOT_FIRST_RUN;
  BrowserInit::LaunchWithProfile launch(FilePath(), dummy, first_run);
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

// Apple Event handler that receives print event from service
// process, gets the required data and launches Print dialog.
- (void)submitCloudPrintJob:(NSAppleEventDescriptor*)event {
  // Pull parameter list out of Apple Event.
  NSAppleEventDescriptor *paramList =
      [event paramDescriptorForKeyword:cloud_print::kAECloudPrintClass];

  if (paramList != nil) {
    // Pull required fields out of parameter list.
    NSString* mime = [[paramList descriptorAtIndex:1] stringValue];
    NSString* inputPath = [[paramList descriptorAtIndex:2] stringValue];
    NSString* printTitle = [[paramList descriptorAtIndex:3] stringValue];
    NSString* printTicket = [[paramList descriptorAtIndex:4] stringValue];
    // Convert the title to UTF 16 as required.
    string16 title16 = base::SysNSStringToUTF16(printTitle);
    string16 printTicket16 = base::SysNSStringToUTF16(printTicket);
    print_dialog_cloud::CreatePrintDialogForFile(
        FilePath([inputPath UTF8String]), title16,
        printTicket16, [mime UTF8String], /*modal=*/false,
        /*delete_on_close=*/false);
  }
}

// Calls the helper class to install the virtual driver to the
// service process.
- (void)installCloudPrint:(NSAppleEventDescriptor*)event {
  cloud_print::VirtualDriverInstallHelper::SetUpInstall();
}

// Calls the helper class to uninstall the virtual driver to the
// service process.
- (void)uninstallCloudPrint:(NSAppleEventDescriptor*)event {
  cloud_print::VirtualDriverInstallHelper::SetUpUninstall();
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

// Show the preferences window, or bring it to the front if it's already
// visible.
- (IBAction)showPreferences:(id)sender {
  if (Browser* browser = ActivateBrowser([self lastProfile])) {
    // Show options tab in the active browser window.
    browser->OpenOptionsDialog();
  } else {
    // No browser window, so create one for the options tab.
    Browser::OpenOptionsWindow([self lastProfile]);
  }
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
        [[AboutWindowController alloc] initWithProfile:[self lastProfile]];

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

- (IBAction)toggleConfirmToQuit:(id)sender {
  PrefService* prefService = [self lastProfile]->GetPrefs();
  bool enabled = prefService->GetBoolean(prefs::kConfirmToQuitEnabled);
  prefService->SetBoolean(prefs::kConfirmToQuitEnabled, !enabled);
}

// Explicitly bring to the foreground when creating new windows from the dock.
- (void)commandFromDock:(id)sender {
  [NSApp activateIgnoringOtherApps:YES];
  [self commandDispatch:sender];
}

- (NSMenu*)applicationDockMenu:(NSApplication*)sender {
  NSMenu* dockMenu = [[[NSMenu alloc] initWithTitle: @""] autorelease];
  Profile* profile = [self lastProfile];

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

- (BookmarkMenuBridge*)bookmarkMenuBridge {
  return bookmarkMenuBridge_.get();
}

- (void)applicationDidChangeScreenParameters:(NSNotification *)notification {
  // During this callback the working area is not always already updated. Defer.
  [self performSelector:@selector(delayedPanelManagerScreenParametersUpdate)
             withObject:nil
             afterDelay:0];
}

- (void)delayedPanelManagerScreenParametersUpdate {
  PanelManager::GetInstance()->OnDisplayChanged();
}

@end  // @implementation AppController

//---------------------------------------------------------------------------

namespace browser {

void ShowInstantConfirmDialog(gfx::NativeWindow parent, Profile* profile) {
  if (Browser* browser = ActivateBrowser(profile)) {
    browser->OpenInstantConfirmDialog();
  } else {
    Browser::OpenInstantConfirmDialogWindow(profile);
  }
}

}  // namespace browser

namespace app_controller_mac {

bool IsOpeningNewWindow() {
  return g_is_opening_new_window;
}

}  // namespace app_controller_mac
