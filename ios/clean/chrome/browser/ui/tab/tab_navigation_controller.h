// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_NAVIGATION_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_NAVIGATION_CONTROLLER_H_

#import <Foundation/Foundation.h>

@class CommandDispatcher;
namespace web {
class WebState;
}

// Object for handling navigation dispatcher calls.
@interface TabNavigationController : NSObject

// The object needs a |dispatcher| in order to register itself as a handler for
// navigations calls; it uses the |webState| to then handle these calls.
- (instancetype)initWithDispatcher:(CommandDispatcher*)dispatcher
                          webState:(web::WebState*)webState;
- (instancetype)init NS_UNAVAILABLE;
// Stops handling navigation dispatcher calls.
- (void)stop;
// The dispatcher used to register and stop listening to navigation calls.
@property(nonatomic, weak) CommandDispatcher* dispatcher;
// The web state this TabNavigationController is using to navigate.
@property(nonatomic, assign) web::WebState* webState;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_TAB_NAVIGATION_CONTROLLER_H_
