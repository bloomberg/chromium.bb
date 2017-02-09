// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_assertions.h"

#import <EarlGrey/EarlGrey.h>

#include "base/format_macros.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/wait_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

void AssertMainTabCount(NSUInteger expected_tab_count) {
  // Allow the UI to become idle, in case any tabs are being opened or closed.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  GREYAssert(testing::WaitUntilConditionOrTimeout(
                 testing::kWaitForUIElementTimeout,
                 ^{
                   return GetMainTabCount() == expected_tab_count;
                 }),
             @"Did not receive %" PRIuNS " tabs", expected_tab_count);
}

void AssertIncognitoTabCount(NSUInteger expected_tab_count) {
  // Allow the UI to become idle, in case any tabs are being opened or closed.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  ConditionBlock condition = ^{
    return GetIncognitoTabCount() == expected_tab_count;
  };
  GREYAssert(testing::WaitUntilConditionOrTimeout(
                 testing::kWaitForUIElementTimeout, condition),
             @"Did not receive %" PRIuNS " incognito tabs", expected_tab_count);
}

}  // namespace chrome_test_util
