// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_CONTROLLER_MAC_H_
#define CHROME_BROWSER_APP_CONTROLLER_MAC_H_

#import <Cocoa/Cocoa.h>
#include <vector>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"

@class AboutWindowController;
class BookmarkMenuBridge;
class CommandUpdater;
@class CrApplication;
class GURL;
class HistoryMenuBridge;
@class PreferencesWindowController;
class Profile;

// The application controller object, created by loading the MainMenu nib.
// This handles things like responding to menus when there are no windows
// open, etc and acts as the NSApplication delegate.
@interface AppController : NSObject<NSUserInterfaceValidations> {
 @private
  scoped_ptr<CommandUpdater> menuState_;
  // Management of the bookmark menu which spans across all windows
  // (and Browser*s).
  scoped_ptr<BookmarkMenuBridge> bookmarkMenuBridge_;
  scoped_ptr<HistoryMenuBridge> historyMenuBridge_;
  PreferencesWindowController* prefsController_;  // Weak.
  AboutWindowController* aboutController_;  // Weak.

  // URLs that need to be opened when the app is fully initialized. Because it's
  // only needed during early startup, it points to a valid vector during early
  // startup and is NULL during the rest of app execution.
  scoped_ptr<std::vector<GURL> > pendingURLs_;

  // Outlets for the close tab/window menu items so that we can adjust the
  // commmand-key equivalent depending on the kind of window and how many
  // tabs it has.
  IBOutlet NSMenuItem* closeTabMenuItem_;
  IBOutlet NSMenuItem* closeWindowMenuItem_;
  BOOL fileMenuUpdatePending_;  // ensure we only do this once per notificaion.

  // Outlet for the help menu so we can bless it so Cocoa adds the search item
  // to it.
  IBOutlet NSMenu* helpMenu_;
}

- (void)didEndMainMessageLoop;
- (Profile*)defaultProfile;

// Show the preferences window, or bring it to the front if it's already
// visible.
- (IBAction)showPreferences:(id)sender;

// Redirect in the menu item from the expected target of "File's
// Owner" (NSAppliation) for a Branded About Box
- (IBAction)orderFrontStandardAboutPanel:(id)sender;

// Delegate method to return the dock menu.
- (NSMenu*)applicationDockMenu:(NSApplication*)sender;

@end

#endif
