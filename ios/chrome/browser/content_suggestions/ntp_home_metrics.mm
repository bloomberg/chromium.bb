// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ntp_home_metrics.h"

#include "base/metrics/histogram_macros.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ntp_home {

void RecordNTPImpression(IOSNTPImpression impression_type) {
  UMA_HISTOGRAM_ENUMERATION("IOS.NTP.Impression", impression_type, COUNT);
}

}  // namespace ntp_home

@interface NTPHomeMetrics ()
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
@end

@implementation NTPHomeMetrics

@synthesize browserState = _browserState;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    _browserState = browserState;
  }
  return self;
}

- (void)recordAction:(new_tab_page_uma::ActionType)action {
  new_tab_page_uma::RecordAction(self.browserState, action);
}

@end
