// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PROFILES_PROFILE_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PROFILES_PROFILE_CHOOSER_CONTROLLER_H_

#import <Cocoa/Cocoa.h>
#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/signin_header_helper.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
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
  // |removeAccount|.
  std::string accountIdToRemove_;

  // Active view mode.
  profiles::BubbleViewMode viewMode_;

  // The current tutorial mode.
  profiles::TutorialMode tutorialMode_;

  // List of the full, un-elided accounts for the active profile. The keys are
  // generated used to tag the UI buttons, and the values are the original
  // emails displayed by the buttons.
  std::map<int, std::string> currentProfileAccounts_;

  // Web contents used by the inline signin view.
  scoped_ptr<content::WebContents> webContents_;

  // Whether the bubble is displayed for an active guest profile.
  BOOL isGuestSession_;

  // The GAIA service type that caused this menu to open.
  signin::GAIAServiceType serviceType_;
}

- (id)initWithBrowser:(Browser*)browser
           anchoredAt:(NSPoint)point
             withMode:(profiles::BubbleViewMode)mode
      withServiceType:(signin::GAIAServiceType)GAIAServiceType;

// Creates all the subviews of the avatar bubble for |viewToDisplay|.
- (void)initMenuContentsWithView:(profiles::BubbleViewMode)viewToDisplay;

// Returns the view currently displayed by the bubble.
- (profiles::BubbleViewMode)viewMode;

// Switches to a given profile. |sender| is an ProfileChooserItemController.
- (IBAction)switchToProfile:(id)sender;

// Shows the User Manager.
- (IBAction)showUserManager:(id)sender;

// Closes all guest browsers and shows the User Manager.
- (IBAction)exitGuest:(id)sender;

// Shows the account management view.
- (IBAction)showAccountManagement:(id)sender;

// Hides the account management view and shows the default view.
- (IBAction)hideAccountManagement:(id)sender;

// Locks the active profile.
- (IBAction)lockProfile:(id)sender;

// Shows the inline signin page.
- (IBAction)showInlineSigninPage:(id)sender;

// Adds an account to the active profile.
- (IBAction)addAccount:(id)sender;

// Shows the account removal view to confirm removing the currently selected
// account from the active profile if possible.
- (IBAction)showAccountRemovalView:(id)sender;

// Shows the account reauthentication view to re-sign in the currently selected
// account from the active profile if possible.
- (IBAction)showAccountReauthenticationView:(id)sender;

// Removes the current account |accountIdToRemove_|.
- (IBAction)removeAccount:(id)sender;

// Reset the WebContents used by the Gaia embedded view.
- (void)cleanUpEmbeddedViewContents;

// Clean-up done after an action was performed in the ProfileChooser.
- (void)postActionPerformed:(ProfileMetrics::ProfileDesktopMenu)action;
@end

// Testing API /////////////////////////////////////////////////////////////////

@interface ProfileChooserController (ExposedForTesting)
- (id)initWithBrowser:(Browser*)browser
           anchoredAt:(NSPoint)point
             withMode:(profiles::BubbleViewMode)mode
      withServiceType:(signin::GAIAServiceType)serviceType;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PROFILES_PROFILE_CHOOSER_CONTROLLER_H_
