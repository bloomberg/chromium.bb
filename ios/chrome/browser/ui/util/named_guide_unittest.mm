// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/named_guide.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using NamedGuideTest = PlatformTest;

// Tests that guides are reachable after being added to a view.
TEST_F(NamedGuideTest, TestAddAndFind) {
  GuideName* test_guide = @"NamedGuideTest";

  UIView* view = [[UIView alloc] init];
  EXPECT_EQ(nil, FindNamedGuide(test_guide, view));

  // The test_guide should be reachable after adding it.
  EXPECT_NE(nil, AddNamedGuide(test_guide, view));
  EXPECT_NE(nil, FindNamedGuide(test_guide, view));
}

// Tests that guides added to a child view are not reachable from the parent.
TEST_F(NamedGuideTest, TestGuideOnChild) {
  GuideName* test_guide = @"NamedGuideTest";

  UIView* view = [[UIView alloc] init];
  UIView* childView = [[UIView alloc] init];
  [view addSubview:childView];

  EXPECT_TRUE(AddNamedGuide(test_guide, childView));
  // This guide should be reachable from the child, but not from the parent.
  EXPECT_TRUE(FindNamedGuide(test_guide, childView));
  EXPECT_FALSE(FindNamedGuide(test_guide, view));
}

// Tests that children can reach guides that are added to ancestors.
TEST_F(NamedGuideTest, TestGuideOnAncestor) {
  GuideName* test_guide = @"NamedGuideTest";

  UIView* view = [[UIView alloc] init];
  UIView* childView = [[UIView alloc] init];
  UIView* grandChildView = [[UIView alloc] init];
  [view addSubview:childView];
  [childView addSubview:grandChildView];

  // Add a guide to the parent view and ensure that it is reachable from all
  // three views.
  EXPECT_TRUE(AddNamedGuide(test_guide, view));
  EXPECT_TRUE(FindNamedGuide(test_guide, grandChildView));
  EXPECT_TRUE(FindNamedGuide(test_guide, childView));
  EXPECT_TRUE(FindNamedGuide(test_guide, view));
}
