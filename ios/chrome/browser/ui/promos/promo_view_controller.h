// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PROMOS_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PROMOS_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/mac/scoped_nsobject.h"

// Protocol for promotion view controllers.
//
// Note: On iPhone, this controller supports portrait orientation only. It
// should always be presented in an |OrientationLimitingNavigationController|.
@protocol PromoViewController

// Returns whether or not a promo of the class type should be displayed.
// Subclasses should override this method. Base implementation returns NO.
+ (BOOL)shouldBePresentedForBrowserState:(ios::ChromeBrowserState*)browserState;

// Returns an autoreleased subclass object, which will be displayed.
// Sublasses should override this method. Base implementation returns nil.
+ (UIViewController*)controllerToPresentForBrowserState:
    (ios::ChromeBrowserState*)browserState;

@end

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_PROMO_VIEW_CONTROLLER_H_
