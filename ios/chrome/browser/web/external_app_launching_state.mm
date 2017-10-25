// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/external_app_launching_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const double kDefaultMaxSecondsBetweenConsecutiveLaunches = 30.0;

namespace {
static double gMaxSecondsBetweenConsecutiveExternalAppLaunches =
    kDefaultMaxSecondsBetweenConsecutiveLaunches;
}  // namespace

@implementation ExternalAppLaunchingState {
  // Timestamp of the last app launch request.
  NSDate* _lastAppLaunchTime;
}
@synthesize consecutiveLaunchesCount = _consecutiveLaunchesCount;
@synthesize appLaunchingBlocked = _appLaunchingBlocked;

+ (void)setMaxSecondsBetweenConsecutiveLaunches:(double)seconds {
  gMaxSecondsBetweenConsecutiveExternalAppLaunches = seconds;
}

- (void)updateWithLaunchRequest {
  if (_appLaunchingBlocked)
    return;
  if (!_lastAppLaunchTime ||
      -_lastAppLaunchTime.timeIntervalSinceNow >
          gMaxSecondsBetweenConsecutiveExternalAppLaunches) {
    _consecutiveLaunchesCount = 1;
  } else {
    _consecutiveLaunchesCount++;
  }
  _lastAppLaunchTime = [NSDate date];
}

@end
