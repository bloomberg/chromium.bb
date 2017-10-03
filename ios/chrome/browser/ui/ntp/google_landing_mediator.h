// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/ntp/google_landing_data_source.h"

@protocol BrowserCommands;
@protocol GoogleLandingConsumer;
@protocol OmniboxFocuser;
@protocol UrlLoader;
class WebStateList;

namespace ios {
class ChromeBrowserState;
}

// A mediator object that provides various data sources for google landing.
@interface GoogleLandingMediator : NSObject<GoogleLandingDataSource>

- (nullable instancetype)
initWithBrowserState:(nonnull ios::ChromeBrowserState*)browserState
        webStateList:(nonnull WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)init NS_UNAVAILABLE;

// Consumer to handle google landing update notifications.
@property(nonatomic, weak, nullable) id<GoogleLandingConsumer> consumer;

// The dispatcher for this mediator.
@property(nonatomic, weak, nullable) id<BrowserCommands, UrlLoader> dispatcher;

// Perform initial setup. Needs to be called before using this object.
- (void)setUp;

// Stop listening to any observers and other cleanup functionality.
- (void)shutdown;

// Get the maximum number of sites shown.
+ (NSUInteger)maxSitesShown;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_MEDIATOR_H_
