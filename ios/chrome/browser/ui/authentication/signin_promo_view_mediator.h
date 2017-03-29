// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"

// Class that configures a SigninPromoView instance. This class is a simple
// implementation that only stores data.
// Create a subclass that monitors profile notification.
@interface SigninPromoViewMediator : NSObject<SigninPromoViewConfigurator>

@property(nonatomic, copy) NSString* userFullName;
@property(nonatomic, copy) NSString* userEmail;
@property(nonatomic, copy) UIImage* userImage;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_MEDIATOR_H_
