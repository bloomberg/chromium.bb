// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PROFILES_PROFILE_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PROFILES_PROFILE_MENU_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include <memory>

class AvatarMenu;
class Browser;

namespace ProfileMenuControllerInternal {
class Observer;
}

// This controller manages the title and submenu of the Profiles item in the
// system menu bar. It updates the contents of the menu and the menu's title
// whenever the active browser changes.
@interface ProfileMenuController : NSObject {
 @private
  // The controller for the profile submenu.
  std::unique_ptr<AvatarMenu> _avatarMenu;

  // An observer to be notified when the active browser changes and when the
  // menu model changes.
  std::unique_ptr<ProfileMenuControllerInternal::Observer> _observer;

  // The main menu item to which the profile menu is attached.
  NSMenuItem* _mainMenuItem;  // weak
}

// Designated initializer.
- (id)initWithMainMenuItem:(NSMenuItem*)item;

// Actions for the menu items.
- (IBAction)switchToProfileFromMenu:(id)sender;
- (IBAction)switchToProfileFromDock:(id)sender;
- (IBAction)editProfile:(id)sender;
- (IBAction)newProfile:(id)sender;

// If profiles are enabled and there is more than one profile, this inserts
// profile menu items into the specified menu at the specified offset and
// returns YES. Otherwise, this returns NO and does not modify the menu.
- (BOOL)insertItemsIntoMenu:(NSMenu*)menu
                   atOffset:(NSInteger)offset
                   fromDock:(BOOL)dock;

@end

@interface ProfileMenuController (PrivateExposedForTesting)
- (NSMenu*)menu;
- (void)rebuildMenu;
- (NSMenuItem*)createItemWithTitle:(NSString*)title action:(SEL)sel;
- (void)activeBrowserChangedTo:(Browser*)browser;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PROFILES_PROFILE_MENU_CONTROLLER_H_
