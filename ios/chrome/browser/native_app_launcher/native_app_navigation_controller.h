// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_NAVIGATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_NAVIGATION_CONTROLLER_H_

#import <Foundation/Foundation.h>

namespace web {
class WebState;
}  // namespace web

// NativeAppNavigationController brings up a GAL Infobar if the webpage directs
// it to do so and there are no other circumstances that would suppress its
// display.
@interface NativeAppNavigationController : NSObject

- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Copies the list of applications possibly being installed and register to be
// notified of their installation.
- (void)copyStateFrom:(NativeAppNavigationController*)controller;
@end

#endif  // IOS_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_NAVIGATION_CONTROLLER_H_
