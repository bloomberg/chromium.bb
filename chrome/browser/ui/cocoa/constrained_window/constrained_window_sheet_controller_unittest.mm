// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "testing/gtest_mac.h"

class ConstrainedWindowSheetControllerTest : public CocoaTest {
 protected:
  ~ConstrainedWindowSheetControllerTest() {
  }

  virtual void SetUp() OVERRIDE {
    CocoaTest::SetUp();

    // Create two dummy tabs and make the first one active.
    NSRect dummyRect = NSMakeRect(0, 0, 50, 50);
    tab_views_.reset([[NSMutableArray alloc] init]);
    for (int i = 0; i < 2; ++i) {
      scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:dummyRect]);
      [tab_views_ addObject:view];
    }
    ActivateTabView([tab_views_ objectAtIndex:0]);

    // Create a test sheet.
    sheet_.reset([[NSWindow alloc] initWithContentRect:dummyRect
                                             styleMask:NSTitledWindowMask
                                               backing:NSBackingStoreBuffered
                                                 defer:NO]);
    [sheet_ setReleasedWhenClosed:NO];

    EXPECT_FALSE([ConstrainedWindowSheetController controllerForSheet:sheet_]);
  }

  virtual void TearDown() OVERRIDE {
    sheet_.reset();
    CocoaTest::TearDown();
  }

  void ActivateTabView(NSView* tab_view) {
    for (NSView* view in tab_views_.get()) {
      [view removeFromSuperview];
    }
    [[test_window() contentView] addSubview:tab_view];
    active_tab_view_.reset([tab_view retain]);

    ConstrainedWindowSheetController* controller =
        [ConstrainedWindowSheetController
            controllerForParentWindow:test_window()];
    EXPECT_TRUE(controller);
    [controller parentViewDidBecomeActive:active_tab_view_];
  }

  scoped_nsobject<NSWindow> sheet_;
  scoped_nsobject<NSMutableArray> tab_views_;
  scoped_nsobject<NSView> active_tab_view_;
};

// Test showing then hiding the sheet.
TEST_F(ConstrainedWindowSheetControllerTest, ShowHide) {
  ConstrainedWindowSheetController* controller =
      [ConstrainedWindowSheetController
          controllerForParentWindow:test_window()];
  EXPECT_TRUE(controller);

  EXPECT_FALSE([sheet_ isVisible]);
  [controller showSheet:sheet_ forParentView:active_tab_view_];
  EXPECT_TRUE([ConstrainedWindowSheetController controllerForSheet:sheet_]);
  EXPECT_TRUE([sheet_ isVisible]);

  [controller closeSheet:sheet_];
  EXPECT_FALSE([ConstrainedWindowSheetController controllerForSheet:sheet_]);
  EXPECT_FALSE([sheet_ isVisible]);
}

// Test that switching tabs correctly hides the inactive tab's sheet.
TEST_F(ConstrainedWindowSheetControllerTest, SwitchTabs) {
  ConstrainedWindowSheetController* controller =
      [ConstrainedWindowSheetController
          controllerForParentWindow:test_window()];
  [controller showSheet:sheet_ forParentView:active_tab_view_];

  EXPECT_TRUE([sheet_ isVisible]);
  EXPECT_EQ(1.0, [sheet_ alphaValue]);
  ActivateTabView([tab_views_ objectAtIndex:1]);
  EXPECT_TRUE([sheet_ isVisible]);
  EXPECT_EQ(0.0, [sheet_ alphaValue]);
  ActivateTabView([tab_views_ objectAtIndex:0]);
  EXPECT_TRUE([sheet_ isVisible]);
  EXPECT_EQ(1.0, [sheet_ alphaValue]);
}

// Test that adding a sheet to an inactive view doesn't show it.
TEST_F(ConstrainedWindowSheetControllerTest, AddToInactiveTab) {
  ConstrainedWindowSheetController* controller =
      [ConstrainedWindowSheetController
          controllerForParentWindow:test_window()];
  NSView* tab0 = [tab_views_ objectAtIndex:0];
  NSView* tab1 = [tab_views_ objectAtIndex:1];

  ActivateTabView(tab0);
  [controller showSheet:sheet_ forParentView:tab1];
  EXPECT_EQ(0.0, [sheet_ alphaValue]);

  ActivateTabView(tab1);
  EXPECT_EQ(1.0, [sheet_ alphaValue]);
}

// Test that two parent windows with two sheet controllers don't conflict.
TEST_F(ConstrainedWindowSheetControllerTest, TwoParentWindows) {
  ConstrainedWindowSheetController* controller1 =
      [ConstrainedWindowSheetController
          controllerForParentWindow:test_window()];
  EXPECT_TRUE(controller1);

  scoped_nsobject<NSWindow> parent_window2([[NSWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, 30, 30)
                styleMask:NSTitledWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  [parent_window2 setReleasedWhenClosed:NO];

  ConstrainedWindowSheetController* controller2 =
      [ConstrainedWindowSheetController
          controllerForParentWindow:parent_window2];
  EXPECT_TRUE(controller2);
  EXPECT_NSNE(controller1, controller2);

  [controller2 showSheet:sheet_ forParentView:[parent_window2 contentView]];
  EXPECT_NSEQ(controller2,
              [ConstrainedWindowSheetController controllerForSheet:sheet_]);

  [parent_window2 close];
}
