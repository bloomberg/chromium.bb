// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_CONTROLLER_MAC_H_
#define CHROME_BROWSER_APP_CONTROLLER_MAC_H_
#pragma once

#import <Cocoa/Cocoa.h>
#include <vector>

#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"

@class AboutWindowController;
class BookmarkMenuBridge;
class CommandUpdater;
class GURL;
class HistoryMenuBridge;
class Profile;

// The application controller object, created by loading the MainMenu nib.
// This handles things like responding to menus when there are no windows
// open, etc and acts as the NSApplication delegate.
@interface AppController : NSObject<NSUserInterfaceValidations,
                                    NSApplicationDelegate> {
 @private
  scoped_ptr<CommandUpdater> menuState_;
  // Management of the bookmark menu which spans across all windows
  // (and Browser*s).
  scoped_ptr<BookmarkMenuBridge> bookmarkMenuBridge_;
  scoped_ptr<HistoryMenuBridge> historyMenuBridge_;
  AboutWindowController* aboutController_;  // Weak.

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

  // Outlet for the tabpose menu item so we can hide it.
  IBOutlet NSMenuItem* tabposeMenuItem_;
}

@property(readonly, nonatomic) BOOL startupComplete;

- (void)didEndMainMessageLoop;
- (Profile*)defaultProfile;

// Try to close all browser windows, and if that succeeds then quit.
- (BOOL)tryToTerminateApplication:(NSApplication*)app;

// Stop trying to terminate the application. That is, prevent the final browser
// window closure from causing the application to quit.
- (void)stopTryingToTerminateApplication:(NSApplication*)app;

// Returns true if there is not a modal window (either window- or application-
// modal) blocking the active browser. Note that tab modal dialogs (HTTP auth
// sheets) will not count as blocking the browser. But things like open/save
// dialogs that are window modal will block the browser.
- (BOOL)keyWindowIsNotModal;

// Show the preferences window, or bring it to the front if it's already
// visible.
- (IBAction)showPreferences:(id)sender;

// Redirect in the menu item from the expected target of "File's
// Owner" (NSAppliation) for a Branded About Box
- (IBAction)orderFrontStandardAboutPanel:(id)sender;

// Toggles the "Confirm to Quit" preference.
- (IBAction)toggleConfirmToQuit:(id)sender;

// Delegate method to return the dock menu.
- (NSMenu*)applicationDockMenu:(NSApplication*)sender;

// Get the URLs that Launch Services expects the browser to open at startup.
- (const std::vector<GURL>&)startupUrls;

// Clear the list of startup URLs.
- (void)clearStartupUrls;

@end

#endif
