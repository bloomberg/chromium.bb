// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/extension_shelf_controller.h"
#import "chrome/browser/cocoa/view_resizer_pong.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class ExtensionShelfControllerTest : public PlatformTest {
 public:
  ExtensionShelfControllerTest() {
    resizeDelegate_.reset([[ViewResizerPong alloc] init]);

    NSRect frame = NSMakeRect(0, 0, 100, 30);
    controller_.reset([[ExtensionShelfController alloc]
        initWithBrowser:helper_.browser()
         resizeDelegate:resizeDelegate_.get()]);
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  BrowserTestHelper helper_;
  scoped_nsobject<ExtensionShelfController> controller_;
  scoped_nsobject<ViewResizerPong> resizeDelegate_;
};

// Check that |hide:| tells the delegate to set the shelf's height to zero.
TEST_F(ExtensionShelfControllerTest, HideSetsHeightToZero) {
  [resizeDelegate_ setHeight:10];
  [controller_ hide:nil];
  EXPECT_EQ(0, [resizeDelegate_ height]);
}

// Check that |show:| tells the delegate to set the shelf's height to the
// shelf's desired height.
TEST_F(ExtensionShelfControllerTest, ShowSetsHeightToHeight) {
  [resizeDelegate_ setHeight:0];
  [controller_ show:nil];
  EXPECT_GT([controller_ height], 0);
  EXPECT_EQ([controller_ height], [resizeDelegate_ height]);
}

// Test adding to the view hierarchy, mostly to ensure nothing leaks or crashes.
TEST_F(ExtensionShelfControllerTest, Add) {
  [cocoa_helper_.contentView() addSubview:[controller_ view]];
  [controller_ wasInsertedIntoWindow];
}

}  // namespace
