// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_INTERACTION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_INTERACTION_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "ios/chrome/browser/signin/constants.h"

namespace ios {
class ChromeBrowserState;
}

// Interaction controller for sign-in related operations. This class is mainly a
// proxy for |ChromeSigninViewController|, calling directly
// |ChromeIdentityInteractionManager| for the no-accounts case.
@interface SigninInteractionController : NSObject

// Designated initializer.
// * |browserState| is the current browser state
// * |presentingViewController| is the top presented view controller.
// * |isPresentedOnSettings| indicates whether the settings view controller is
//   part of the presented view controllers stack
// * |accessPoint| represents the access point that initiated the sign-in.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
            presentingViewController:(UIViewController*)presentingViewController
               isPresentedOnSettings:(BOOL)isPresentedOnSettings
                   signInAccessPoint:(signin_metrics::AccessPoint)accessPoint;

// Starts user sign-in. The completion block will be called when the operation
// is done, and |succeeded| will notify the caller on whether the user is now
// signed in.
- (void)signInWithCompletion:(signin_ui::CompletionCallback)completion
              viewController:(UIViewController*)viewController;

// Re-authenticate the user. This method will always show a sign-in web flow.
// The completion block will be called when the operation is done, and
// |succeeded| will notify the caller on whether the user has been
// correctly re-authenticated.
- (void)reAuthenticateWithCompletion:(signin_ui::CompletionCallback)completion
                      viewController:(UIViewController*)viewController;

// Starts the flow to add an identity via ChromeIdentityInteractionManager.
- (void)addAccountWithCompletion:(signin_ui::CompletionCallback)completion
                  viewController:(UIViewController*)viewController;

// Cancels any current process. Calls the completion callback when done.
- (void)cancel;

// Cancels any current process and dismisses any UI. Calls the completion
// callback when done.
- (void)cancelAndDismiss;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_INTERACTION_CONTROLLER_H_
