// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using ConstraintsUIUtilTest = PlatformTest;

// Tests that SafeAreaLayoutGuideForView returns and attach a single
// safeAreaLayoutGuide before iOS 11.
TEST_F(ConstraintsUIUtilTest, SafeAreaLayoutGuideForView) {
  if (@available(iOS 11, *)) {
  } else {
    UIView* view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
    NSUInteger originalLayoutGuideCount = [view.layoutGuides count];

    // Check that calling |SafeAreaLayoutGuideForView| increases the
    // layout guides associated with |view|.
    UILayoutGuide* safeArea1 = SafeAreaLayoutGuideForView(view);
    EXPECT_EQ(originalLayoutGuideCount + 1, [view.layoutGuides count]);

    // Check that calling |SafeAreaLayoutGuideForView| again does not
    // increase the layout guides associated with |view|.
    UILayoutGuide* safeArea2 = SafeAreaLayoutGuideForView(view);
    EXPECT_EQ(originalLayoutGuideCount + 1, [view.layoutGuides count]);

    // Check that multiple calls to |SafeAreaLayoutGuideForView| return the
    // same object.
    EXPECT_EQ(safeArea1, safeArea2);
  }
}

}  // namespace
