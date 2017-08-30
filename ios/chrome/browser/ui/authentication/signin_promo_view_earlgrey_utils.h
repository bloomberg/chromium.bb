// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGN_PROMO_VIEW_EARLGREY_UTILS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGN_PROMO_VIEW_EARLGREY_UTILS_H_

#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"

@interface SignPromoViewEarlgreyUtils : NSObject

// Checks that the sign-in promo view is visible using the right mode.
+ (void)checkSigninPromoVisibleWithMode:(SigninPromoViewMode)mode;

// Checks that the sign-in promo view is not visible.
+ (void)checkSigninPromoNotVisible;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGN_PROMO_VIEW_EARLGREY_UTILS_H_
