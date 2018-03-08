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
  EXPECT_EQ(nil, [NamedGuide guideWithName:test_guide view:view]);

  // The test_guide should be reachable after adding it.
  NamedGuide* guide = [[NamedGuide alloc] initWithName:test_guide];
  [view addLayoutGuide:guide];
  EXPECT_EQ(guide, [NamedGuide guideWithName:test_guide view:view]);
}

// Tests that guides added to a child view are not reachable from the parent.
TEST_F(NamedGuideTest, TestGuideOnChild) {
  GuideName* test_guide = @"NamedGuideTest";

  UIView* view = [[UIView alloc] init];
  UIView* childView = [[UIView alloc] init];
  [view addSubview:childView];

  NamedGuide* guide = [[NamedGuide alloc] initWithName:test_guide];
  [childView addLayoutGuide:guide];

  // This guide should be reachable from the child, but not from the parent.
  EXPECT_EQ(guide, [NamedGuide guideWithName:test_guide view:childView]);
  EXPECT_EQ(nil, [NamedGuide guideWithName:test_guide view:view]);
}

// Tests that children can reach guides that are added to ancestors.
TEST_F(NamedGuideTest, TestGuideOnAncestor) {
  GuideName* test_guide = @"NamedGuideTest";

  UIView* view = [[UIView alloc] init];
  UIView* childView = [[UIView alloc] init];
  UIView* grandChildView = [[UIView alloc] init];
  [view addSubview:childView];
  [childView addSubview:grandChildView];

  NamedGuide* guide = [[NamedGuide alloc] initWithName:test_guide];
  [view addLayoutGuide:guide];

  // The guide added to the top-level view should be accessible from all
  // descendent views.
  EXPECT_EQ(guide, [NamedGuide guideWithName:test_guide view:grandChildView]);
  EXPECT_EQ(guide, [NamedGuide guideWithName:test_guide view:childView]);
  EXPECT_EQ(guide, [NamedGuide guideWithName:test_guide view:view]);
}

// Tests that resetting the constrained view updates the guide.
TEST_F(NamedGuideTest, TestConstrainedViewUpdate) {
  GuideName* test_guide = @"NamedGuideTest";

  UIWindow* window =
      [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  UIView* view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  [window addSubview:view];
  [view addSubview:[[UIView alloc] initWithFrame:CGRectMake(0, 0, 50, 100)]];
  [view addSubview:[[UIView alloc] initWithFrame:CGRectMake(50, 0, 50, 100)]];

  NamedGuide* guide = [[NamedGuide alloc] initWithName:test_guide];
  [view addLayoutGuide:guide];

  // Set the constrained view to the subviews and verify that the layout frame
  // is updated.
  for (UIView* subview in view.subviews) {
    guide.constrainedView = subview;
    [view setNeedsLayout];
    [view layoutIfNeeded];
    EXPECT_TRUE(CGRectEqualToRect(guide.layoutFrame, subview.frame));
  }
}
