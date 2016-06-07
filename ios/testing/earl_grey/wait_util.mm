// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/testing/earl_grey/wait_util.h"

#import <EarlGrey/EarlGrey.h>

#include "base/test/ios/wait_util.h"

namespace testing {

const NSTimeInterval kSpinDelaySeconds = 0.01;
const NSTimeInterval kWaitForJSCompletionTimeout = 2.0;
const NSTimeInterval kWaitForUIElementTimeout = 4.0;

void WaitUntilCondition(NSTimeInterval timeout, bool (^condition)(void)) {
  NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:timeout];
  while (!condition() &&
         [[NSDate date] compare:deadline] != NSOrderedDescending) {
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::TimeDelta::FromSecondsD(testing::kSpinDelaySeconds));
  }
  GREYAssert(condition(), @"Timeout waiting for condition.");
}

}  // namespace testing
