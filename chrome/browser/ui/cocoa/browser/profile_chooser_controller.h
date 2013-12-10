// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_PROFILE_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_PROFILE_CHOOSER_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

class AvatarMenu;
class Browser;

// This window controller manages the bubble that displays a "menu" of profiles.
// It is brought open by clicking on the avatar icon in the window frame.
@interface ProfileChooserController : BaseBubbleController {
 @private
  // The menu that contains the data from the backend.
  scoped_ptr<AvatarMenu> avatarMenu_;

  // The browser that launched the bubble. Not owned.
  Browser* browser_;
}

- (id)initWithBrowser:(Browser*)browser anchoredAt:(NSPoint)point;

// Creates a new profile.
- (IBAction)addNewProfile:(id)sender;

// Switches to a given profile. |sender| is an ProfileChooserItemController.
- (IBAction)switchToProfile:(id)sender;

// Shows the User Manager.
- (IBAction)showUserManager:(id)sender;

// Starts a guest browser window.
- (IBAction)switchToGuestProfile:(id)sender;

// Closes all guest browser windows.
- (IBAction)exitGuestProfile:(id)sender;

// Shows the account management view.
- (IBAction)showAccountManagement:(id)sender;

// Locks the active profile.
- (IBAction)lockProfile:(id)sender;

// Shows the signin page.
- (IBAction)showSigninPage:(id)sender;

@end

// Testing API /////////////////////////////////////////////////////////////////

@interface ProfileChooserController (ExposedForTesting)
- (id)initWithBrowser:(Browser*)browser anchoredAt:(NSPoint)point;
@end

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_PROFILE_CHOOSER_CONTROLLER_H_
