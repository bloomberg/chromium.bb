// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_PROVIDER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_PROVIDER_H_

#import <UIKit/UIKit.h>

@protocol LogoVendor;

// Interface providing a header view for the NTP Header.
@protocol NTPHomeHeaderProvider

// Exposes view and methods to drive the doodle.
@property(nonatomic, weak, nullable) id<LogoVendor> logoVendor;

- (nullable UIView*)headerForWidth:(CGFloat)width;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_PROVIDER_H_
