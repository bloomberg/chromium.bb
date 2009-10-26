// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/extension_shelf_controller.h"
#import "chrome/browser/cocoa/view_resizer_pong.h"

namespace {

class ExtensionShelfControllerTest : public CocoaTest {
 public:
  ExtensionShelfControllerTest() {
    resizeDelegate_.reset([[ViewResizerPong alloc] init]);

    NSRect frame = NSMakeRect(0, 0, 100, 30);
    controller_.reset([[ExtensionShelfController alloc]
        initWithBrowser:helper_.browser()
         resizeDelegate:resizeDelegate_.get()]);
    NSView* view = [controller_ view];
    EXPECT_TRUE(view);
    [[test_window() contentView] addSubview:view];
  }

  BrowserTestHelper helper_;
  scoped_nsobject<ExtensionShelfController> controller_;
  scoped_nsobject<ViewResizerPong> resizeDelegate_;
};

TEST_VIEW(ExtensionShelfControllerTest, [controller_ view]);

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

}  // namespace
