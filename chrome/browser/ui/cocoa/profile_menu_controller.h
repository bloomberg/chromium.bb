// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PROFILE_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PROFILE_MENU_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"

class Browser;
@class MenuController;
class ProfileMenuModel;

namespace ProfileMenuControllerInternal {
class Observer;
}

// This controller manages the title and submenu of the Profiles item in the
// system menu bar. It updates the contents of the menu and the menu's title
// whenever the active browser changes.
@interface ProfileMenuController : NSObject {
 @private
  // The model for the profile submenu.
  scoped_ptr<ProfileMenuModel> submenuModel_;

  // The Cocoa controller that creates the NSMenu from the model.
  scoped_nsobject<MenuController> submenuController_;

  // A BrowserList::Observer to be notified when the active browser changes.
  scoped_ptr<ProfileMenuControllerInternal::Observer> observer_;

  // The main menu item to which the profile menu is attached.
  __weak NSMenuItem* mainMenuItem_;
}

// Designated initializer.
- (id)initWithMainMenuItem:(NSMenuItem*)item;

@end

#endif  // CHROME_BROWSER_UI_COCOA_PROFILE_MENU_CONTROLLER_H_
