// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/page_info_window_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class PageInfoWindowControllerTest : public PlatformTest {
 public:
  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  BrowserTestHelper helper_;
};


TEST_F(PageInfoWindowControllerTest, TestImages) {
  scoped_nsobject<PageInfoWindowController>
    controller([[PageInfoWindowController alloc] init]);
  [controller window];  // Force nib load.
  EXPECT_TRUE([controller goodImg]);
  EXPECT_TRUE([controller badImg]);
}


TEST_F(PageInfoWindowControllerTest, TestGrow) {
  scoped_nsobject<PageInfoWindowController>
    controller([[PageInfoWindowController alloc] init]);
  [controller window];  // Force nib load.
  NSRect frame = [[controller window] frame];
  [controller setShowHistoryBox:YES];
  NSRect newFrame = [[controller window] frame];
  EXPECT_GE(newFrame.size.height, frame.size.height);
  EXPECT_LE(newFrame.origin.y, frame.origin.y);
}


TEST_F(PageInfoWindowControllerTest, TestShrink) {
  scoped_nsobject<PageInfoWindowController>
    controller([[PageInfoWindowController alloc] init]);
  [controller window];  // Force nib to load.
  [controller setShowHistoryBox:YES];
  NSRect frame = [[controller window] frame];
  [controller setShowHistoryBox:NO];
  NSRect newFrame = [[controller window] frame];
  EXPECT_LE(newFrame.size.height, frame.size.height);
  EXPECT_GE(newFrame.origin.y, frame.origin.y);
}

