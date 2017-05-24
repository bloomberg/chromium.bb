// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_CONSUMER_H_

#import <Foundation/Foundation.h>

@class ChromeIdentity;
@class SigninPromoViewMediator;

// Handles identity update notifications.
@protocol SigninPromoViewConsumer<NSObject>

// Called when the default identity is changed or updated.
// |configurator|, new instance set each time, to configure a SigninPromoView.
// |identityChanged| is set to YES when the default identity is changed.
- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_CONSUMER_H_
