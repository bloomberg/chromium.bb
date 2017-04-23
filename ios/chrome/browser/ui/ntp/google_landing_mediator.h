// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/ntp/google_landing_data_source.h"

@protocol GoogleLandingConsumer;
@protocol OmniboxFocuser;
@protocol UrlLoader;

namespace ios {
class ChromeBrowserState;
}

// A mediator object that provides various data sources for google landing.
@interface GoogleLandingMediator : NSObject<GoogleLandingDataSource>

- (instancetype)initWithConsumer:(id<GoogleLandingConsumer>)consumer
                    browserState:(ios::ChromeBrowserState*)browserState
                          loader:(id<UrlLoader>)loader
                         focuser:(id<OmniboxFocuser>)focuser
              webToolbarDelegate:(id<WebToolbarDelegate>)webToolbarDelegate
                        tabModel:(TabModel*)tabModel NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Get the maximum number of sites shown.
+ (NSUInteger)maxSitesShown;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_MEDIATOR_H_
