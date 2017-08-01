// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_MEDIATOR_H_

#import <UIKit/UIKit.h>

@protocol NTPConsumer;

// A mediator object that sets an NTP view controller's appearance based on
// various data sources.
@interface NTPMediator : NSObject
- (instancetype)initWithConsumer:(id<NTPConsumer>)consumer
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_MEDIATOR_H_
