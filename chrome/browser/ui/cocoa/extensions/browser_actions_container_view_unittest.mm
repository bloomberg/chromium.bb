// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_container_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

const CGFloat kContainerHeight = 15.0;
const CGFloat kMinimumContainerWidth = 10.0;

class BrowserActionsContainerViewTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    view_.reset([[BrowserActionsContainerView alloc]
        initWithFrame:NSMakeRect(0, 0, 0, kContainerHeight)]);
  }

  base::scoped_nsobject<BrowserActionsContainerView> view_;
};

TEST_F(BrowserActionsContainerViewTest, BasicTests) {
  EXPECT_TRUE([view_ isResizable]);
  EXPECT_TRUE([view_ canDragLeft]);
  EXPECT_TRUE([view_ canDragRight]);
  EXPECT_TRUE([view_ isHidden]);
}

TEST_F(BrowserActionsContainerViewTest, SetWidthTests) {
  // Try setting below the minimum width (10 pixels).
  [view_ resizeToWidth:5.0 animate:NO];
  EXPECT_EQ(kMinimumContainerWidth, NSWidth([view_ frame])) << "Frame width is "
      << "less than the minimum allowed.";
  // Since the frame expands to the left, the x-position delta value will be
  // negative.
  EXPECT_EQ(-kMinimumContainerWidth, [view_ resizeDeltaX]);

  [view_ resizeToWidth:35.0 animate:NO];
  EXPECT_EQ(35.0, NSWidth([view_ frame]));
  EXPECT_EQ(-25.0, [view_ resizeDeltaX]);

  [view_ resizeToWidth:20.0 animate:NO];
  EXPECT_EQ(20.0, NSWidth([view_ frame]));
  EXPECT_EQ(15.0, [view_ resizeDeltaX]);
}

}  // namespace
