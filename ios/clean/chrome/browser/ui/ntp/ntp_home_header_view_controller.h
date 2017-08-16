// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/browser/ui/ntp/ntp_home_header_consumer.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_home_header_provider.h"

// Coordinator handling the header of the NTP home panel.
@interface NTPHomeHeaderViewController
    : UIViewController<NTPHomeHeaderConsumer, NTPHomeHeaderProvider>

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_VIEW_CONTROLLER_H_
