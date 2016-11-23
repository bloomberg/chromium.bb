// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/wait_util.h"

#include "base/test/ios/wait_util.h"

namespace testing {

const NSTimeInterval kSpinDelaySeconds = 0.01;
const NSTimeInterval kWaitForJSCompletionTimeout = 2.0;
const NSTimeInterval kWaitForUIElementTimeout = 4.0;
const NSTimeInterval kWaitForDownloadTimeout = 10.0;
const NSTimeInterval kWaitForPageLoadTimeout = 10.0;

bool WaitUntilConditionOrTimeout(NSTimeInterval timeout,
                                 ConditionBlock condition) {
  NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:timeout];
  bool success = condition();
  while (!success && [[NSDate date] compare:deadline] != NSOrderedDescending) {
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::TimeDelta::FromSecondsD(testing::kSpinDelaySeconds));
    success = condition();
  }
  return success;
}

}  // namespace testing
