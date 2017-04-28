// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/ntp/google_landing_data_source.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"

@protocol ChromeExecuteCommand;
@protocol GoogleLandingConsumer;
@protocol OmniboxFocuser;
@protocol UrlLoader;
class WebStateList;

namespace ios {
class ChromeBrowserState;
}

// A mediator object that provides various data sources for google landing.
@interface GoogleLandingMediator : NSObject<GoogleLandingDataSource>

- (instancetype)initWithConsumer:(id<GoogleLandingConsumer>)consumer
                    browserState:(ios::ChromeBrowserState*)browserState
                      dispatcher:(id<ChromeExecuteCommand, UrlLoader>)dispatcher
                    webStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Stop listening to any observers and other cleanup functionality.
- (void)shutdown;

// Get the maximum number of sites shown.
+ (NSUInteger)maxSitesShown;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_MEDIATOR_H_
