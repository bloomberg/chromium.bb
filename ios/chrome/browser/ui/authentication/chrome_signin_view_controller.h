// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CHROME_SIGNIN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CHROME_SIGNIN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "components/signin/core/browser/signin_metrics.h"
#import "ios/chrome/browser/signin/constants.h"

@class ChromeSigninViewController;
@class GenericChromeCommand;
@class ChromeIdentity;

namespace ios {
class ChromeBrowserState;
}  // namespace ios

@protocol ChromeSigninViewControllerDelegate<NSObject>

// Informs the delegate that a sign-in operation will start in |controller|.
- (void)willStartSignIn:(ChromeSigninViewController*)controller;

// Informs the delegate that an add account operation will start in
// |controller|.
- (void)willStartAddAccount:(ChromeSigninViewController*)controller;

// Informs the delegate that the user skipped sign-in in |controller|.
- (void)didSkipSignIn:(ChromeSigninViewController*)controller;

// Informs the delegate that the sign-in failed in |controller|.
- (void)didFailSignIn:(ChromeSigninViewController*)controller;

// Informs the delegate that the user was signed in in |controller|.
//
// Note that the sign-in flow did not actually finish when this method is
// called. The delegate will be called with [-didAcceptSignIn:] or
// [-didUndoSignIn:identity:] once the user decides to accept or to undo the
// sign-in operation.
- (void)didSignIn:(ChromeSigninViewController*)controller;

// Informs the delegate that the sign-in of |identity| was undone in
// |controller|.
// |identity| will be automatically forgotten if it was added during this
// sign-in flow.
- (void)didUndoSignIn:(ChromeSigninViewController*)controller
             identity:(ChromeIdentity*)identity;

// Informs the delegate that the user has accepted the sign-in in |controller|.
// If |command| is not nil, the delegate is expected to execute it.
// This marks the end of the sign-in flow.
- (void)didAcceptSignIn:(ChromeSigninViewController*)controller
         executeCommand:(GenericChromeCommand*)command;

@end

// ChromeSigninViewController is a view controller that handles all the
// sign-in UI flow.
@interface ChromeSigninViewController : UIViewController

@property(nonatomic, assign) id<ChromeSigninViewControllerDelegate> delegate;

// Data clearing policy to use during the authentication.
// It is valid to set this in the |willStartSignIn:| method of the delegate.
@property(nonatomic, assign) ShouldClearData shouldClearData;

// Designated initializer.
// * |browserState| is the current browser state.
// * |isPresentedOnSettings| indicates whether the settings view controller is
//   part of the presented view controllers stack.
// * |accessPoint| represents the access point that initiated the sign-in.
// * |identity| will be signed in without requiring user input if not nil.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
               isPresentedOnSettings:(BOOL)isPresentedOnSettings
                         accessPoint:(signin_metrics::AccessPoint)accessPoint
                         promoAction:(signin_metrics::PromoAction)promoAction
                      signInIdentity:(ChromeIdentity*)identity;

// Cancels the on-going authentication operation (if any). |delegate| will be
// called with |didFailSignIn|.
- (void)cancel;

@end

@interface ChromeSigninViewController (Subclassing)

@property(nonatomic, readonly) ios::ChromeBrowserState* browserState;

@property(nonatomic, readonly) UIColor* backgroundColor;

// Vertical padding used underneath buttons. Default value is 18.
@property(nonatomic, assign) CGFloat buttonVerticalPadding;

// Primary button title used to accept the sign-in.
@property(nonatomic, readonly) NSString* acceptSigninButtonTitle;

// Secondary button title used to skip the sign-in.
@property(nonatomic, readonly) NSString* skipSigninButtonTitle;

@property(nonatomic, readonly) UIButton* primaryButton;
@property(nonatomic, readonly) UIButton* secondaryButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CHROME_SIGNIN_VIEW_CONTROLLER_H_
