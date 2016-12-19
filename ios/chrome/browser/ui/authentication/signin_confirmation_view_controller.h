// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONFIRMATION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONFIRMATION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"

@class ChromeIdentity;
@class SigninConfirmationViewController;

@protocol SigninConfirmationViewControllerDelegate

// Informs the delegate that the link to open the sync settings was tapped.
- (void)signinConfirmationControllerDidTapSettingsLink:
    (SigninConfirmationViewController*)controller;

// Informs the delegate that the confirmation view has been scrolled all
// the way to the bottom.
- (void)signinConfirmationControllerDidReachBottom:
    (SigninConfirmationViewController*)controller;

@end

// Controller of the sign-in confirmation collection view.
@interface SigninConfirmationViewController : CollectionViewController

- (instancetype)initWithIdentity:(ChromeIdentity*)identity
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

// Scrolls the confirmation view to the bottom of its content.
- (void)scrollToBottom;

@property(nonatomic, assign) id<SigninConfirmationViewControllerDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONFIRMATION_VIEW_CONTROLLER_H_
