// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/authentication_flow.h"

// Delegate that interacts with the user sign-in coordinator.
@protocol UserSigninViewControllerDelegate

// Returns whether the user has selected an identity from the unified consent
// screen.
- (BOOL)unifiedConsentCoordinatorHasIdentity;

// Performs add account operation.
- (void)userSigninViewControllerDidTapOnAddAccount;

// Performs scroll operation on unified consent screen.
- (void)userSigninViewControllerDidScrollOnUnifiedConsent;

// Performs operations to skip sign-in or undo existing sign-in.
- (void)userSigninViewControllerDidTapOnSkipSignin;

// Performs operations to skip sign-in or undo existing sign-in.
- (void)userSigninViewControllerDidTapOnSignin;

@end

// View controller used to show sign-in UI.
@interface UserSigninViewController
    : UIViewController <AuthenticationFlowDelegate>

// The delegate.
@property(nonatomic, weak) id<UserSigninViewControllerDelegate> delegate;

// View controller that handles the user consent before the user signs in.
@property(nonatomic, weak) UIViewController* unifiedConsentViewController;

@property(nonatomic, assign, readonly) int acceptSigninButtonStringId;

@property(nonatomic, assign) BOOL useFirstRunSkipButton;

// Informs the view controller that the unified consent has reached the bottom
// of the screen.
- (void)markUnifiedConsentScreenReachedBottom;

// Updates the primary button based on the user sign-in state.
- (void)updatePrimaryButtonStyle;

// Returns the supported orientations for the device type:
// |UIInterfaceOrientationPortrait| orientation on iPhone and all other
// orientations on iPad.
- (NSUInteger)supportedInterfaceOrientations;

// Blocks the UI (except the cancel button) when the sign-in is in progress.
- (void)signinWillStart;

// Unblocks the UI when the sign-in is done.
- (void)signinDidStop;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_USER_SIGNIN_USER_SIGNIN_VIEW_CONTROLLER_H_
