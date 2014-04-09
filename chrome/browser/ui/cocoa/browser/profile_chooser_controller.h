// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_PROFILE_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_PROFILE_CHOOSER_CONTROLLER_H_

#import <Cocoa/Cocoa.h>
#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

class AvatarMenu;
class ActiveProfileObserverBridge;
class Browser;
class ProfileOAuth2TokenService;

namespace content {
class WebContents;
}

// This window controller manages the bubble that displays a "menu" of profiles.
// It is brought open by clicking on the avatar icon in the window frame.
@interface ProfileChooserController : BaseBubbleController<NSTextViewDelegate> {
 @public
  // Different views that can be displayed in the bubble.
  enum BubbleViewMode {
    // Shows a "fast profile switcher" view.
    BUBBLE_VIEW_MODE_PROFILE_CHOOSER,
    // Shows a list of accounts for the active user.
    BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT,
    // Shows a web view for primary sign in.
    BUBBLE_VIEW_MODE_GAIA_SIGNIN,
    // Shows a web view for adding secondary accounts.
    BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT,
    // Shows a view for confirming account removal.
    BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL
  };

 @private
  // The menu that contains the data from the backend.
  scoped_ptr<AvatarMenu> avatarMenu_;

  // An observer to be notified when the OAuth2 tokens change or the avatar
  // menu model updates for the active profile.
  scoped_ptr<ActiveProfileObserverBridge> observer_;

  // The browser that launched the bubble. Not owned.
  Browser* browser_;

  // The id for the account that the user has requested to remove from the
  // current profile. It is set in |showAccountRemovalView| and used in
  // |removeAccountAndRelaunch|.
  std::string accountIdToRemove_;

  // Active view mode.
  BubbleViewMode viewMode_;

  // Whether the tutorial card is showing in the last active view.
  BOOL tutorialShowing_;

  // List of the full, un-elided accounts for the active profile. The keys are
  // generated used to tag the UI buttons, and the values are the original
  // emails displayed by the buttons.
  std::map<int, std::string> currentProfileAccounts_;

  // Web contents used by the inline signin view.
  scoped_ptr<content::WebContents> webContents_;

  // Whether the bubble is displayed for an active guest profile.
  BOOL isGuestSession_;
}

- (id)initWithBrowser:(Browser*)browser
           anchoredAt:(NSPoint)point
             withMode:(BubbleViewMode)mode;

// Creates all the subviews of the avatar bubble for |viewToDisplay|.
- (void)initMenuContentsWithView:(BubbleViewMode)viewToDisplay;

// Returns the view currently displayed by the bubble.
- (BubbleViewMode)viewMode;

// Switches to a given profile. |sender| is an ProfileChooserItemController.
- (IBAction)switchToProfile:(id)sender;

// Shows the User Manager.
- (IBAction)showUserManager:(id)sender;

// Shows the account management view.
- (IBAction)showAccountManagement:(id)sender;

// Locks the active profile.
- (IBAction)lockProfile:(id)sender;

// Shows the signin page.
- (IBAction)showSigninPage:(id)sender;

// Adds an account to the active profile.
- (IBAction)addAccount:(id)sender;

// Shows the account removal view to confirm removing the currently selected
// account from the active profile if possible.
- (IBAction)showAccountRemovalView:(id)sender;

// Removes the current account |accountIdToRemove_| and relaunches the browser.
- (IBAction)removeAccountAndRelaunch:(id)sender;

// Reset the WebContents used by the Gaia embedded view.
- (void)cleanUpEmbeddedViewContents;
@end

// Testing API /////////////////////////////////////////////////////////////////

@interface ProfileChooserController (ExposedForTesting)
- (id)initWithBrowser:(Browser*)browser
           anchoredAt:(NSPoint)point
             withMode:(BubbleViewMode)mode;
@end

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_PROFILE_CHOOSER_CONTROLLER_H_
