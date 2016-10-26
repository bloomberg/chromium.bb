// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/testing/earl_grey/wait_util.h"

#import <EarlGrey/EarlGrey.h>

#include "base/test/ios/wait_util.h"
#include "ios/testing/wait_util.h"

namespace testing {

void WaitUntilCondition(NSTimeInterval timeout,
                        NSString* timeoutDescription,
                        bool (^condition)(void)) {
  GREYAssert(testing::WaitUntilConditionOrTimeout(timeout, condition),
             timeoutDescription);
}

void WaitUntilCondition(NSTimeInterval timeout, bool (^condition)(void)) {
  WaitUntilCondition(timeout, @"Timeout waiting for condition.", condition);
}

}  // namespace testing
