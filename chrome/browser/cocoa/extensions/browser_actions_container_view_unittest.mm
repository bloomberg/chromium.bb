// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/extensions/browser_actions_container_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

const CGFloat kContainerHeight = 15.0;

class BrowserActionsContainerViewTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    view_.reset([[BrowserActionsContainerView alloc]
        initWithFrame:NSMakeRect(0, 0, 0, kContainerHeight)]);
  }

  scoped_nsobject<BrowserActionsContainerView> view_;
};

TEST_F(BrowserActionsContainerViewTest, BasicTests) {
  EXPECT_TRUE([view_ canDragLeft]);
  EXPECT_TRUE([view_ canDragRight]);
  EXPECT_TRUE([view_ isHidden]);
}

}  // namespace
