// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_wk_navigation_handler.h"

#include "base/timer/timer.h"
#import "ios/web/navigation/crw_pending_navigation_info.h"
#import "ios/web/navigation/crw_wk_navigation_states.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWWKNavigationHandler {
  // Used to poll for a SafeBrowsing warning being displayed. This is created in
  // |decidePolicyForNavigationAction| and destroyed once any of the following
  // happens: 1) a SafeBrowsing warning is detected; 2) any WKNavigationDelegate
  // method is called; 3) |stopLoading| is called.
  base::RepeatingTimer _safeBrowsingWarningDetectionTimer;
}

- (instancetype)init {
  if (self = [super init]) {
    _navigationStates = [[CRWWKNavigationStates alloc] init];
  }
  return self;
}

#pragma mark - Public methods

- (void)stopLoading {
  self.pendingNavigationInfo.cancelled = YES;
  _safeBrowsingWarningDetectionTimer.Stop();
}

- (base::RepeatingTimer*)safeBrowsingWarningDetectionTimer {
  return &_safeBrowsingWarningDetectionTimer;
}

@end
