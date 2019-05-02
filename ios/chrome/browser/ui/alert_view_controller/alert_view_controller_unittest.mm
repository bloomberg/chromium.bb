// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/alert_view_controller/alert_view_controller.h"

#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using AlertViewControllerTest = PlatformTest;

// Tests AlertViewController can be initiliazed.
TEST_F(AlertViewControllerTest, Init) {
  AlertViewController* alert = [[AlertViewController alloc] init];
  EXPECT_TRUE(alert);
}

// Tests there are no circular references in a simple init.
TEST_F(AlertViewControllerTest, Dealloc) {
  __weak AlertViewController* weakAlert = nil;
  @autoreleasepool {
    AlertViewController* alert = [[AlertViewController alloc] init];
    weakAlert = alert;
  }
  EXPECT_FALSE(weakAlert);
}
