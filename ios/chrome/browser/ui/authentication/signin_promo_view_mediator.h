// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"

@class ChromeIdentity;
@class SigninPromoViewMediator;

// Handles identity update notifications.
@protocol SigninPromoViewMediatorDelegate<NSObject>

- (void)signinPromoViewMediatorCurrentIdentityUpdated:
    (SigninPromoViewMediator*)signinPromoViewMediator;
- (void)signinPromoViewMediatorCurrentIdentityChanged:
    (SigninPromoViewMediator*)signinPromoViewMediator;

@end

// Class that configures a SigninPromoView based on the identities known by
// ChromeIdentityService. If at least one identity is found, the view will be
// configured in a warm state mode (the user will be invited to continue without
// typing its password). Otherwise, the view will be configured in a cold state
// mode.
@interface SigninPromoViewMediator : NSObject<SigninPromoViewConfigurator>

// Delegate to handles identity update notification.
@property(nonatomic, weak) id<SigninPromoViewMediatorDelegate> delegate;

// Chrome identity used to configure the view in a warm state mode. Otherwise
// contains nil.
@property(nonatomic, readonly, strong) ChromeIdentity* defaultIdentity;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_MEDIATOR_H_
