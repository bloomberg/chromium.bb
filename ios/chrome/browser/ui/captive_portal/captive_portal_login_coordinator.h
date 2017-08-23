// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CAPTIVE_PORTAL_CAPTIVE_PORTAL_LOGIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_CAPTIVE_PORTAL_CAPTIVE_PORTAL_LOGIN_COORDINATOR_H_

#import "ios/chrome/browser/chrome_coordinator.h"

class GURL;

// A coordinator for displaying a captive portal login page.
@interface CaptivePortalLoginCoordinator : ChromeCoordinator

// Initializes a coordinator for displaying a captive portal login page on
// |viewController| and displaying |landingURL|.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                landingURL:(const GURL&)landingURL
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_CAPTIVE_PORTAL_CAPTIVE_PORTAL_LOGIN_COORDINATOR_H_
