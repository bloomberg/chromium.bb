// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/page_info_window_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"

class PageInfoWindowControllerTest : public CocoaTest {
  virtual void SetUp() {
    CocoaTest::SetUp();
    controller_ = [[PageInfoWindowController alloc] init];
    EXPECT_TRUE([controller_ window]);
  }

  virtual void TearDown() {
    [controller_ close];
    CocoaTest::TearDown();
  }

 public:
  BrowserTestHelper helper_;
  PageInfoWindowController* controller_;
};


TEST_F(PageInfoWindowControllerTest, TestImages) {
  EXPECT_TRUE([controller_ goodImg]);
  EXPECT_TRUE([controller_ badImg]);
}


TEST_F(PageInfoWindowControllerTest, TestGrow) {
  NSRect frame = [[controller_ window] frame];
  [controller_ setShowHistoryBox:YES];
  NSRect newFrame = [[controller_ window] frame];
  EXPECT_GE(newFrame.size.height, frame.size.height);
  EXPECT_LE(newFrame.origin.y, frame.origin.y);
}


TEST_F(PageInfoWindowControllerTest, TestShrink) {
  [controller_ setShowHistoryBox:YES];
  NSRect frame = [[controller_ window] frame];
  [controller_ setShowHistoryBox:NO];
  NSRect newFrame = [[controller_ window] frame];
  EXPECT_LE(newFrame.size.height, frame.size.height);
  EXPECT_GE(newFrame.origin.y, frame.origin.y);
}
