// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PROMOS_SIGNIN_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PROMOS_SIGNIN_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/chrome_signin_view_controller.h"

@protocol ApplicationSettingsCommands;
namespace ios {
class ChromeBrowserState;
}

// Key in the UserDefaults to record the version of the application when the
// SSO Recall promo has been displayed.
// Exposed for testing.
extern NSString* kDisplayedSSORecallForMajorVersionKey;

// Class to display a promotion view to encourage the user to sign on, if
// SSO detects that the user has signed in with another application.
//
// Note: On iPhone, this controller supports portrait orientation only. It
// should always be presented in an |OrientationLimitingNavigationController|.
@interface SigninPromoViewController : ChromeSigninViewController

// YES if this promo should be shown for |browserState|
+ (BOOL)shouldBePresentedForBrowserState:(ios::ChromeBrowserState*)browserState;

// Designated initializer.  |browserState| must not be nil.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                          dispatcher:
                              (id<ApplicationSettingsCommands>)dispatcher;

// Records in user defaults that the promo has been shown along with the current
// version number.
+ (void)recordVersionSeen;

@end

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_SIGNIN_PROMO_VIEW_CONTROLLER_H_
