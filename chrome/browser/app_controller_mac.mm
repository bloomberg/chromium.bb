// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/app_controller_mac.h"

#include "app/l10n_util_mac.h"
#include "base/command_line.h"
#include "base/mac_util.h"
#include "base/message_loop.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browser_window.h"
#import "chrome/browser/chrome_application_mac.h"
#import "chrome/browser/cocoa/about_window_controller.h"
#import "chrome/browser/cocoa/bookmark_menu_bridge.h"
#import "chrome/browser/cocoa/browser_window_cocoa.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/bug_report_window_controller.h"
#import "chrome/browser/cocoa/history_menu_bridge.h"
#import "chrome/browser/cocoa/clear_browsing_data_controller.h"
#import "chrome/browser/cocoa/encoding_menu_controller_delegate_mac.h"
#import "chrome/browser/cocoa/preferences_window_controller.h"
#import "chrome/browser/cocoa/tab_strip_controller.h"
#import "chrome/browser/cocoa/tab_window_controller.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/temp_scaffolding_stubs.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

@interface AppController(PRIVATE)
- (void)initMenuState;
- (void)openURLs:(const std::vector<GURL>&)urls;
- (void)openPendingURLs;
- (void)getUrl:(NSAppleEventDescriptor*)event
     withReply:(NSAppleEventDescriptor*)reply;
- (void)openFiles:(NSAppleEventDescriptor*)event
        withReply:(NSAppleEventDescriptor*)reply;
- (void)windowLayeringDidChange:(NSNotification*)inNotification;
- (BOOL)userWillWaitForInProgressDownloads:(int)downloadCount;
- (BOOL)shouldQuitWithInProgressDownloads;
@end

// True while AppController is calling Browser::OpenEmptyWindow(). We need a
// global flag here, analogue to BrowserInit::InProcessStartup() because
// otherwise the SessionService will try to restore sessions when we make a new
// window while there are no other active windows.
static bool g_is_opening_new_window = false;

@implementation AppController

// This method is called very early in application startup (ie, before
// the profile is loaded or any preferences have been registered). Defer any
// user-data initialization until -applicationDidFinishLaunching:.
- (void)awakeFromNib {
  pendingURLs_.reset(new std::vector<GURL>());

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
  [em setEventHandler:self
          andSelector:@selector(openFiles:withReply:)
        forEventClass:kCoreEventClass
           andEventID:kAEOpenDocuments];

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

- (NSApplicationTerminateReply)applicationShouldTerminate:
    (NSApplication *)sender {
  // Check for in-progress downloads, and prompt the user if they really want to
  // quit (and thus cancel the downloads).
  if (![self shouldQuitWithInProgressDownloads])
    return NSTerminateCancel;

  return NSTerminateNow;
}

// Called when the app is shutting down. Clean-up as appropriate.
- (void)applicationWillTerminate:(NSNotification *)aNotification {
  NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
  [em removeEventHandlerForEventClass:kInternetEventClass
                           andEventID:kAEGetURL];
  [em removeEventHandlerForEventClass:'WWW!'
                           andEventID:'OURL'];
  [em removeEventHandlerForEventClass:kCoreEventClass
                           andEventID:kAEOpenDocuments];

  // Close all the windows.
  BrowserList::CloseAllBrowsers(true);

  // On Windows, this is done in Browser::OnWindowClosing, but that's not
  // appropriate on Mac since we don't shut down when we reach zero windows.
  browser_shutdown::OnShutdownStarting(browser_shutdown::BROWSER_EXIT);

  // Release the reference to the browser process. Once all the browsers get
  // dealloc'd, it will stop the RunLoop and fall back into main().
  g_browser_process->ReleaseModule();

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
// Close Tab/Close Window accordingly
- (void)fixCloseMenuItemKeyEquivalents {
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
  fileMenuUpdatePending_ = NO;
}

// Fix up the "close tab/close window" command-key equivalents. We do this
// after a delay to ensure that window layer state has been set by the time
// we do the enabling.
- (void)delayedFixCloseMenuItemKeyEquivalents {
  if (!fileMenuUpdatePending_) {
    // The OS prefers keypresses to timers, so it's possible that a cmd-w
    // can sneak in before this timer fires. In order to prevent that from
    // having any bad consequences, just clear the keys combos altogether. They
    // will be reset when the timer eventually fires.
    [self clearCloseMenuItemKeyEquivalents];
    [self performSelector:@selector(fixCloseMenuItemKeyEquivalents)
               withObject:nil
               afterDelay:0];
    fileMenuUpdatePending_ = YES;
  }
}

// Called when we get a notification about the window layering changing to
// update the UI based on the new main window.
- (void)windowLayeringDidChange:(NSNotification*)notify {
  [self delayedFixCloseMenuItemKeyEquivalents];

  // TODO(pinkerton): If we have other things here, such as inspector panels
  // that follow the contents of the selected webpage, we would update those
  // here.
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
    NSNumber *value = [NSNumber numberWithFloat:fiveHoursInSeconds];
    CFPreferencesSetAppValue(checkInterval, value, app);
    CFPreferencesAppSynchronize(app);
  }
#endif
}

// This is called after profiles have been loaded and preferences registered.
// It is safe to access the default profile here.
- (void)applicationDidFinishLaunching:(NSNotification*)notify {
  // Hold an extra ref to the BrowserProcess singleton so it doesn't go away
  // when all the browser windows get closed. We'll release it on quit which
  // will be the signal to exit.
  DCHECK(g_browser_process);
  g_browser_process->AddRefModule();

  bookmarkMenuBridge_.reset(new BookmarkMenuBridge([self defaultProfile]));
  historyMenuBridge_.reset(new HistoryMenuBridge([self defaultProfile]));

  [self setUpdateCheckInterval];

  // Build up the encoding menu, the order of the items differs based on the
  // current locale (see http://crbug.com/7647 for details).
  // We need a valid g_browser_process to get the profile which is why we can't
  // call this from awakeFromNib.
  NSMenu* view_menu = [[[NSApp mainMenu] itemWithTag:IDC_VIEW_MENU] submenu];
  NSMenuItem* encoding_menu_item = [view_menu itemWithTag:IDC_ENCODING_MENU];
  NSMenu *encoding_menu = [encoding_menu_item submenu];
  EncodingMenuControllerDelegate::BuildEncodingMenu([self defaultProfile],
                                                    encoding_menu);

  // Now that we're initialized we can open any URLs we've been holding onto.
  [self openPendingURLs];
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

  std::wstring product_name = l10n_util::GetString(IDS_PRODUCT_NAME);

  // Set the dialog text based on whether or not there are multiple downloads.
  if (downloadCount == 1) {
    // Dialog text: warning and explanation.
    warningText =
        base::SysWideToNSString(l10n_util::GetStringF(
            IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_WARNING, product_name));
    explanationText =
        base::SysWideToNSString(l10n_util::GetStringF(
            IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_EXPLANATION, product_name));

    // Cancel download and exit button text.
    exitTitle =
        base::SysWideToNSString(l10n_util::GetString(
            IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_OK_BUTTON_LABEL));

    // Wait for download button text.
    waitTitle =
        base::SysWideToNSString(l10n_util::GetString(
            IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_CANCEL_BUTTON_LABEL));
  } else {
    // Dialog text: warning and explanation.
    warningText =
        base::SysWideToNSString(l10n_util::GetStringF(
            IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_WARNING, product_name,
            IntToWString(downloadCount)));
    explanationText =
        base::SysWideToNSString(l10n_util::GetStringF(
            IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_EXPLANATION, product_name));

    // Cancel downloads and exit button text.
    exitTitle =
        base::SysWideToNSString(l10n_util::GetString(
            IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_OK_BUTTON_LABEL));

    // Wait for downloads button text.
    waitTitle =
        base::SysWideToNSString(l10n_util::GetString(
            IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_CANCEL_BUTTON_LABEL));
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

// Returns true if there is no browser window, or if the active window is
// blocked by a modal dialog.
- (BOOL)keyWindowIsMissingOrBlocked {
  Browser* browser = BrowserList::GetLastActive();
  return browser == NULL ||
         ![[browser->window()->GetNativeHandle() attachedSheet]
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
  if (action == @selector(commandDispatch:)) {
    NSInteger tag = [item tag];
    if (menuState_->SupportsCommand(tag)) {
      switch (tag) {
        case IDC_RESTORE_TAB:
          enable = [self keyWindowIsMissingOrBlocked] && [self canRestoreTab];
          break;
        // The File Menu commands are not automatically disabled by Cocoa when
        // a dialog sheet obscures the browser window, so we disable them here.
        // We don't need to include IDC_CLOSE_WINDOW, because app_controller
        // is only activated when there are no key windows (see function
        // comment).
        case IDC_OPEN_FILE:
        case IDC_NEW_WINDOW:
        case IDC_NEW_TAB:
        case IDC_NEW_INCOGNITO_WINDOW:
          enable = [self keyWindowIsMissingOrBlocked];
          break;
        default:
          enable = menuState_->IsCommandEnabled(tag) ? YES : NO;
      }
    }
  } else if (action == @selector(terminate:)) {
    enable = YES;
  } else if (action == @selector(showPreferences:)) {
    enable = YES;
  } else if (action == @selector(orderFrontStandardAboutPanel:)) {
    enable = YES;
  } else if (action == @selector(newWindowFromDock:)) {
    enable = YES;
  }
  return enable;
}

// Called when the user picks a menu item when there are no key windows. Calls
// through to the browser object to execute the command. This assumes that the
// command is supported and doesn't check, otherwise it would have been disabled
// in the UI in validateUserInterfaceItem:.
- (void)commandDispatch:(id)sender {
  Profile* defaultProfile = [self defaultProfile];

  NSInteger tag = [sender tag];
  switch (tag) {
    case IDC_NEW_TAB:
    case IDC_NEW_WINDOW:
    case IDC_FOCUS_LOCATION:
      g_is_opening_new_window = true;
      Browser::OpenEmptyWindow(defaultProfile);
      g_is_opening_new_window = false;
      break;
    case IDC_NEW_INCOGNITO_WINDOW:
      Browser::OpenEmptyWindow(defaultProfile->GetOffTheRecordProfile());
      break;
    case IDC_RESTORE_TAB:
      Browser::OpenWindowWithRestoredTabs(defaultProfile);
      break;
    case IDC_OPEN_FILE:
      Browser::OpenEmptyWindow(defaultProfile);
      BrowserList::GetLastActive()->
          ExecuteCommandWithDisposition(IDC_OPEN_FILE, CURRENT_TAB);
      break;
    case IDC_CLEAR_BROWSING_DATA: {
      // There may not be a browser open, so use the default profile.
      scoped_nsobject<ClearBrowsingDataController> controller(
          [[ClearBrowsingDataController alloc]
              initWithProfile:defaultProfile]);
      [controller runModalDialog];
      break;
    }
    case IDC_SHOW_HISTORY:
      Browser::OpenHistoryWindow(defaultProfile);
      break;
    case IDC_SHOW_DOWNLOADS:
      Browser::OpenDownloadsWindow(defaultProfile);
      break;
    case IDC_HELP_PAGE:
      Browser::OpenHelpWindow(defaultProfile);
      break;
    case IDC_REPORT_BUG: {
      Browser* browser = BrowserList::GetLastActive();
      TabContents* current_tab = (browser != NULL) ?
          browser->GetSelectedTabContents() : NULL;
      BugReportWindowController* controller =
          [[BugReportWindowController alloc]
          initWithTabContents:current_tab
                      profile:[self defaultProfile]];
      [controller runModalDialog];
      break;
    }
  };
}

// NSApplication delegate method called when someone clicks on the
// dock icon and there are no open windows.  To match standard mac
// behavior, we should open a new window.
- (BOOL)applicationShouldHandleReopen:(NSApplication*)theApplication
                    hasVisibleWindows:(BOOL)flag {
  // Don't do anything if there are visible windows.  This will cause
  // AppKit to unminimize the most recently minimized window.
  if (flag)
    return YES;

  // Otherwise open a new window.
  g_is_opening_new_window = true;
  Browser::OpenEmptyWindow([self defaultProfile]);
  g_is_opening_new_window = false;

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
  menuState_->UpdateCommandEnabled(IDC_SHOW_HISTORY, true);
  menuState_->UpdateCommandEnabled(IDC_SHOW_DOWNLOADS, true);
  menuState_->UpdateCommandEnabled(IDC_HELP_PAGE, true);
  menuState_->UpdateCommandEnabled(IDC_REPORT_BUG, true);
  // TODO(pinkerton): ...more to come...
}

- (Profile*)defaultProfile {
  // TODO(jrg): Find a better way to get the "default" profile.
  if (g_browser_process->profile_manager())
    return* g_browser_process->profile_manager()->begin();

  return NULL;
}

// Various methods to open URLs that we get in a native fashion. We use
// BrowserInit here because on the other platforms, URLs to open come through
// the ProcessSingleton, and it calls BrowserInit. It's best to bottleneck the
// openings through that for uniform handling.

- (void)openURLs:(const std::vector<GURL>&)urls {
  if (pendingURLs_.get()) {
    // too early to open; save for later
    pendingURLs_->insert(pendingURLs_->end(), urls.begin(), urls.end());
    return;
  }

  Browser* browser = BrowserList::GetLastActive();
  // if no browser window exists then create one with no tabs to be filled in
  if (!browser) {
    browser = Browser::Create([self defaultProfile]);
    browser->window()->Show();
  }

  CommandLine dummy(CommandLine::ARGUMENTS_ONLY);
  BrowserInit::LaunchWithProfile launch(std::wstring(), dummy);
  launch.OpenURLsInBrowser(browser, false, urls);
}

- (void)openPendingURLs {
  // Since the existence of pendingURLs_ is a flag that it's too early to
  // open URLs, we need to reset pendingURLs_.
  std::vector<GURL> urls;
  swap(urls, *pendingURLs_);
  pendingURLs_.reset();

  if (urls.size())
    [self openURLs:urls];
}

- (void)getUrl:(NSAppleEventDescriptor*)event
     withReply:(NSAppleEventDescriptor*)reply {
  NSString* urlStr = [[event paramDescriptorForKeyword:keyDirectObject]
                      stringValue];

  GURL gurl(base::SysNSStringToUTF8(urlStr));
  std::vector<GURL> gurlVector;
  gurlVector.push_back(gurl);

  [self openURLs:gurlVector];
}

- (void)openFiles:(NSAppleEventDescriptor*)event
        withReply:(NSAppleEventDescriptor*)reply {
  // Ordinarily we'd use the NSApplication delegate method
  // -application:openFiles:, but Cocoa tries to be smart and it sends files
  // specified on the command line into that delegate method. That's too smart
  // for us (our setup isn't done by the time Cocoa triggers the delegate method
  // and we crash). Since all we want are files dropped on the app icon, and we
  // have cross-platform code to handle the command-line files anyway, an Apple
  // Event handler fits the bill just right.
  NSAppleEventDescriptor* fileList =
      [event paramDescriptorForKeyword:keyDirectObject];
  if (!fileList)
    return;
  std::vector<GURL> gurlVector;

  for (NSInteger i = 1; i <= [fileList numberOfItems]; ++i) {
    NSAppleEventDescriptor* fileAliasDesc = [fileList descriptorAtIndex:i];
    if (!fileAliasDesc)
      continue;
    NSAppleEventDescriptor* fileURLDesc =
        [fileAliasDesc coerceToDescriptorType:typeFileURL];
    if (!fileURLDesc)
      continue;
    NSData* fileURLData = [fileURLDesc data];
    if (!fileURLData)
      continue;
    GURL gurl(std::string((char*)[fileURLData bytes], [fileURLData length]));
    gurlVector.push_back(gurl);
  }

  [self openURLs:gurlVector];
}

// Called when the preferences window is closed. We use this to release the
// window controller.
- (void)prefsWindowClosed:(NSNotification*)notify {
  [[NSNotificationCenter defaultCenter]
    removeObserver:self
              name:kUserDoneEditingPrefsNotification
            object:prefsController_.get()];
  prefsController_.reset(NULL);
}

// Show the preferences window, or bring it to the front if it's already
// visible.
- (IBAction)showPreferences:(id)sender {
  if (!prefsController_.get()) {
    prefsController_.reset([[PreferencesWindowController alloc]
                              initWithProfile:[self defaultProfile]]);
    // Watch for a notification of when it goes away so that we can destroy
    // the controller.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(prefsWindowClosed:)
               name:kUserDoneEditingPrefsNotification
             object:prefsController_.get()];
  }
  [prefsController_ showPreferences:sender];
}

// Called when the about window is closed. We use this to release the
// window controller.
- (void)aboutWindowClosed:(NSNotification*)notify {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:kUserClosedAboutNotification
              object:aboutController_.get()];
  aboutController_.reset(nil);
}

- (IBAction)orderFrontStandardAboutPanel:(id)sender {
  if (!aboutController_) {
    aboutController_.reset([[AboutWindowController alloc]
                            initWithProfile:[self defaultProfile]]);

    // Watch for a notification of when it goes away so that we can destroy
    // the controller.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(aboutWindowClosed:)
               name:kUserClosedAboutNotification
             object:aboutController_.get()];
  }

  if (![[aboutController_ window] isVisible])
    [[aboutController_ window] center];

  [aboutController_ showWindow:self];
}

// Explicitly bring to the foreground when creating new windows from the dock.
- (void)newWindowFromDock:(id)sender {
  [NSApp activateIgnoringOtherApps:YES];
  [self commandDispatch:sender];
}

- (NSMenu*)applicationDockMenu:(id)sender {
  NSMenu* dockMenu = [[[NSMenu alloc] initWithTitle: @""] autorelease];
  NSString* titleStr = l10n_util::GetNSStringWithFixup(IDS_NEW_WINDOW_MAC);
  scoped_nsobject<NSMenuItem> item([[NSMenuItem alloc]
                                       initWithTitle:titleStr
                                       action:@selector(newWindowFromDock:)
                                       keyEquivalent:@""]);
  [item setTarget:self];
  [item setTag:IDC_NEW_WINDOW];
  [dockMenu addItem:item];

  titleStr = l10n_util::GetNSStringWithFixup(IDS_NEW_INCOGNITO_WINDOW_MAC);
  item.reset([[NSMenuItem alloc] initWithTitle:titleStr
                                 action:@selector(newWindowFromDock:)
                                 keyEquivalent:@""]);
  [item setTarget:self];
  [item setTag:IDC_NEW_INCOGNITO_WINDOW];
  [dockMenu addItem:item];

  return dockMenu;
}

@end

//---------------------------------------------------------------------------

// Stub for cross-platform method that isn't called on Mac OS X.
void ShowOptionsWindow(OptionsPage page,
                       OptionsGroup highlight_group,
                       Profile* profile) {
  NOTIMPLEMENTED();
}

namespace app_controller_mac {

bool IsOpeningNewWindow() {
  return g_is_opening_new_window;
}

}  // namespace app_controller_mac
