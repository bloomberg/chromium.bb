// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/centering_scrollview.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class CenteringScrollViewTest : public PlatformTest {
 protected:
  void SetUp() override {
    scrollView_ = [[CenteringScrollView alloc] initWithFrame:CGRectZero];
    contentView_ = [[UIView alloc] initWithFrame:CGRectZero];
  };

  CenteringScrollView* scrollView_;
  UIView* contentView_;
};

TEST_F(CenteringScrollViewTest, CenteringContent) {
  // Set up content view to be smaller than the scrolling view.
  [contentView_ setFrame:CGRectMake(0.0, 0.0, 80.0, 120.0)];
  [scrollView_ addSubview:contentView_];
  // Do test.
  [scrollView_ setFrame:CGRectMake(0.0, 0.0, 320, 480.0)];
  [scrollView_ layoutSubviews];
  // Verify placement of content view.
  CGRect r = [contentView_ frame];
  EXPECT_EQ(120.0, r.origin.x);
  EXPECT_EQ(180.0, r.origin.y);
  // Verify that scroller content is set to not scrollable.
  EXPECT_EQ(0.0, [scrollView_ contentSize].width);
  EXPECT_EQ(0.0, [scrollView_ contentSize].height);
  EXPECT_EQ(0.0, [scrollView_ contentOffset].x);
  EXPECT_EQ(0.0, [scrollView_ contentOffset].y);
}

TEST_F(CenteringScrollViewTest, ScrollingContent) {
  // Set up content view taller than the scrolling view.
  [contentView_ setFrame:CGRectMake(0.0, 0.0, 240.0, 400.0)];
  [scrollView_ addSubview:contentView_];
  // Do test
  [scrollView_ setFrame:CGRectMake(0.0, 0.0, 480.0, 320.0)];
  [scrollView_ layoutSubviews];
  // Verify placement of content view.
  CGRect r = [contentView_ frame];
  EXPECT_EQ(120.0, r.origin.x);
  EXPECT_EQ(0.0, r.origin.y);
  // Verify that scroller content is resized correctly.
  EXPECT_EQ(480.0, [scrollView_ contentSize].width);
  EXPECT_EQ(400.0, [scrollView_ contentSize].height);
  EXPECT_EQ(0.0, [scrollView_ contentOffset].x);
  EXPECT_EQ(0.0, [scrollView_ contentOffset].y);
}

TEST_F(CenteringScrollViewTest, ResetContentSize) {
  // Set up content view taller than the scrolling view.
  [contentView_ setFrame:CGRectMake(0.0, 0.0, 240.0, 400.0)];
  [scrollView_ addSubview:contentView_];

  [scrollView_ setFrame:CGRectMake(0.0, 0.0, 280.0, 440.0)];
  [scrollView_ layoutSubviews];
  // Verify that scroller content is set to not scrollable.
  EXPECT_EQ(0.0, [scrollView_ contentSize].width);
  EXPECT_EQ(0.0, [scrollView_ contentSize].height);

  [scrollView_ setFrame:CGRectMake(0.0, 0.0, 200.0, 300.0)];
  [scrollView_ layoutSubviews];
  // Verify that scroller content is equal to the content view size.
  EXPECT_EQ(240.0, [scrollView_ contentSize].width);
  EXPECT_EQ(400.0, [scrollView_ contentSize].height);

  [scrollView_ setFrame:CGRectMake(0.0, 0.0, 260.0, 420.0)];
  [scrollView_ layoutSubviews];
  // Verify that scroller content is set to not scrollable.
  EXPECT_EQ(0.0, [scrollView_ contentSize].width);
  EXPECT_EQ(0.0, [scrollView_ contentSize].height);
}

}  // anonymous namespace
