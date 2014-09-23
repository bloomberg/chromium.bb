// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_CONTROLLER_MAC_H_
#define CHROME_BROWSER_APP_CONTROLLER_MAC_H_

#if defined(__OBJC__)

#import <Cocoa/Cocoa.h>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/time/time.h"
#include "ui/base/work_area_watcher_observer.h"

class AppControllerProfileObserver;
@class AppShimMenuController;
class BookmarkMenuBridge;
class CommandUpdater;
class GURL;
class HistoryMenuBridge;
class Profile;
@class ProfileMenuController;
class QuitWithAppsController;

namespace ui {
class WorkAreaWatcherObserver;
}

// The application controller object, created by loading the MainMenu nib.
// This handles things like responding to menus when there are no windows
// open, etc and acts as the NSApplication delegate.
@interface AppController : NSObject<NSUserInterfaceValidations,
                                    NSApplicationDelegate> {
 @private
  // Manages the state of the command menu items.
  scoped_ptr<CommandUpdater> menuState_;

  // The profile last used by a Browser. It is this profile that was used to
  // build the user-data specific main menu items.
  Profile* lastProfile_;

  // The ProfileObserver observes the ProfileInfoCache and gets notified
  // when a profile has been deleted.
  scoped_ptr<AppControllerProfileObserver> profileInfoCacheObserver_;

  // Management of the bookmark menu which spans across all windows
  // (and Browser*s).
  scoped_ptr<BookmarkMenuBridge> bookmarkMenuBridge_;
  scoped_ptr<HistoryMenuBridge> historyMenuBridge_;

  // Controller that manages main menu items for packaged apps.
  base::scoped_nsobject<AppShimMenuController> appShimMenuController_;

  // The profile menu, which appears right before the Help menu. It is only
  // available when multiple profiles is enabled.
  base::scoped_nsobject<ProfileMenuController> profileMenuController_;

  // If we're told to open URLs (in particular, via |-application:openFiles:| by
  // Launch Services) before we've launched the browser, we queue them up in
  // |startupUrls_| so that they can go in the first browser window/tab.
  std::vector<GURL> startupUrls_;
  BOOL startupComplete_;

  // Outlets for the close tab/window menu items so that we can adjust the
  // commmand-key equivalent depending on the kind of window and how many
  // tabs it has.
  IBOutlet NSMenuItem* closeTabMenuItem_;
  IBOutlet NSMenuItem* closeWindowMenuItem_;
  BOOL fileMenuUpdatePending_;  // ensure we only do this once per notificaion.

  // Outlet for the help menu so we can bless it so Cocoa adds the search item
  // to it.
  IBOutlet NSMenu* helpMenu_;

  // Indicates wheter an NSPopover is currently being shown.
  BOOL hasPopover_;

  // If we are expecting a workspace change in response to a reopen
  // event, the time we got the event. A null time otherwise.
  base::TimeTicks reopenTime_;

  // Observers that listen to the work area changes.
  ObserverList<ui::WorkAreaWatcherObserver> workAreaChangeObservers_;

  scoped_ptr<PrefChangeRegistrar> profilePrefRegistrar_;
  PrefChangeRegistrar localPrefRegistrar_;

  // Displays a notification when quitting while apps are running.
  scoped_refptr<QuitWithAppsController> quitWithAppsController_;
}

@property(readonly, nonatomic) BOOL startupComplete;
@property(readonly, nonatomic) Profile* lastProfile;

- (void)didEndMainMessageLoop;

// Try to close all browser windows, and if that succeeds then quit.
- (BOOL)tryToTerminateApplication:(NSApplication*)app;

// Stop trying to terminate the application. That is, prevent the final browser
// window closure from causing the application to quit.
- (void)stopTryingToTerminateApplication:(NSApplication*)app;

// Returns true if there is a modal window (either window- or application-
// modal) blocking the active browser. Note that tab modal dialogs (HTTP auth
// sheets) will not count as blocking the browser. But things like open/save
// dialogs that are window modal will block the browser.
- (BOOL)keyWindowIsModal;

// Show the preferences window, or bring it to the front if it's already
// visible.
- (IBAction)showPreferences:(id)sender;

// Redirect in the menu item from the expected target of "File's
// Owner" (NSApplication) for a Branded About Box
- (IBAction)orderFrontStandardAboutPanel:(id)sender;

// Toggles the "Confirm to Quit" preference.
- (IBAction)toggleConfirmToQuit:(id)sender;

// Toggles the "Hide Notifications Icon" preference.
- (IBAction)toggleDisplayMessageCenter:(id)sender;

// Delegate method to return the dock menu.
- (NSMenu*)applicationDockMenu:(NSApplication*)sender;

// Get the URLs that Launch Services expects the browser to open at startup.
- (const std::vector<GURL>&)startupUrls;

- (BookmarkMenuBridge*)bookmarkMenuBridge;

// Subscribes/unsubscribes from the work area change notification.
- (void)addObserverForWorkAreaChange:(ui::WorkAreaWatcherObserver*)observer;
- (void)removeObserverForWorkAreaChange:(ui::WorkAreaWatcherObserver*)observer;

// Initializes the AppShimMenuController. This enables changing the menu bar for
// apps.
- (void)initAppShimMenuController;

// Called when the user has changed browser windows, meaning the backing profile
// may have changed. This can cause a rebuild of the user-data menus. This is a
// no-op if the new profile is the same as the current one. This will always be
// the original profile and never incognito.
- (void)windowChangedToProfile:(Profile*)profile;

@end

#endif  // __OBJC__

// Functions that may be accessed from non-Objective-C C/C++ code.

namespace app_controller_mac {

// True if we are currently handling an IDC_NEW_{TAB,WINDOW} command. Used in
// SessionService::Observe() to get around windows/linux and mac having
// different models of application lifetime.
bool IsOpeningNewWindow();

}  // namespace app_controller_mac

#endif
