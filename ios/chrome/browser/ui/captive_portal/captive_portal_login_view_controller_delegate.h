// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CAPTIVE_PORTAL_CAPTIVE_PORTAL_LOGIN_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CAPTIVE_PORTAL_CAPTIVE_PORTAL_LOGIN_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

@class CaptivePortalLoginViewController;

// Delegate for CaptivePortalLoginViewController.
@protocol CaptivePortalLoginViewControllerDelegate<NSObject>

// Notifies the delegate that the user is done with the |controller|.
- (void)captivePortalLoginViewControllerDidFinish:
    (CaptivePortalLoginViewController*)controller;

@end

#endif  // IOS_CHROME_BROWSER_UI_CAPTIVE_PORTAL_CAPTIVE_PORTAL_LOGIN_VIEW_CONTROLLER_DELEGATE_H_
