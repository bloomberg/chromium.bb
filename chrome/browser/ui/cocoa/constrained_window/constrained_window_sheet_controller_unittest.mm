// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"

#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "testing/gtest_mac.h"

namespace {

const int kSystemSheetReturnCode = 77;

}  // namespace

@interface ConstrainedWindowSystemSheetTest
    : NSObject <ConstrainedWindowSheet> {
  int returnCode_;
  NSAlert* alert_;  // weak
}

@property(nonatomic, readonly) int returnCode;
@property(nonatomic, assign) NSAlert* alert;

@end

@implementation ConstrainedWindowSystemSheetTest

@synthesize returnCode = returnCode_;
@synthesize alert = alert_;

- (void)showSheetForWindow:(NSWindow*)window {
  [alert_ beginSheetModalForWindow:window
                     modalDelegate:self
                    didEndSelector:@selector(alertDidEnd:returnCode:ctxInfo:)
                       contextInfo:NULL];
}

- (void)closeSheetWithAnimation:(BOOL)withAnimation {
  [NSApp endSheet:[alert_ window] returnCode:kSystemSheetReturnCode];
}

- (void)hideSheet {
}

- (void)unhideSheet {
}

- (void)pulseSheet {
}

- (void)makeSheetKeyAndOrderFront {
}

- (void)updateSheetPosition {
}

- (void)alertDidEnd:(NSAlert *)alert
         returnCode:(NSInteger)returnCode
            ctxInfo:(void *)contextInfo {
  returnCode_ = returnCode;
}

@end

class ConstrainedWindowSheetControllerTest : public CocoaTest {
 protected:
  virtual ~ConstrainedWindowSheetControllerTest() {
  }

  virtual void SetUp() OVERRIDE {
    CocoaTest::SetUp();

    // Center the window so that the sheet doesn't go offscreen.
    [test_window() center];

    // Create two dummy tabs and make the first one active.
    NSRect dummyRect = NSMakeRect(0, 0, 50, 50);
    tab_views_.reset([[NSMutableArray alloc] init]);
    for (int i = 0; i < 2; ++i) {
      base::scoped_nsobject<NSView> view(
          [[NSView alloc] initWithFrame:dummyRect]);
      [tab_views_ addObject:view];
    }
    tab0_.reset([[tab_views_ objectAtIndex:0] retain]);
    tab1_.reset([[tab_views_ objectAtIndex:1] retain]);
    ActivateTabView(tab0_);

    // Create a test sheet.
    sheet_window_.reset([[NSWindow alloc]
        initWithContentRect:dummyRect
                  styleMask:NSTitledWindowMask
                    backing:NSBackingStoreBuffered
                      defer:NO]);
    [sheet_window_ setReleasedWhenClosed:NO];
    sheet_.reset([[CustomConstrainedWindowSheet alloc]
        initWithCustomWindow:sheet_window_]);

    controller_.reset([[ConstrainedWindowSheetController
            controllerForParentWindow:test_window()] retain]);
    EXPECT_TRUE(controller_);
    EXPECT_FALSE([ConstrainedWindowSheetController controllerForSheet:sheet_]);
  }

  virtual void TearDown() OVERRIDE {
    sheet_.reset();
    sheet_window_.reset();
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

  NSRect GetViewFrameInScreenCoordinates(NSView* view) {
    NSRect rect = [view convertRect:[view bounds] toView:nil];
    rect.origin = [[view window] convertBaseToScreen:rect.origin];
    return rect;
  }

  void VerifySheetXPosition(NSRect sheet_frame, NSView* parent_view) {
    NSRect parent_frame = GetViewFrameInScreenCoordinates(parent_view);
    CGFloat expected_x = NSMinX(parent_frame) +
        (NSWidth(parent_frame) - NSWidth(sheet_frame)) / 2.0;
    EXPECT_EQ(expected_x, NSMinX(sheet_frame));
  }

  CGFloat GetSheetYOffset(NSRect sheet_frame, NSView* parent_view) {
    return NSMaxY(sheet_frame) -
           NSMaxY(GetViewFrameInScreenCoordinates(parent_view));
  }

  base::scoped_nsobject<NSWindow> sheet_window_;
  base::scoped_nsobject<CustomConstrainedWindowSheet> sheet_;
  base::scoped_nsobject<ConstrainedWindowSheetController> controller_;
  base::scoped_nsobject<NSMutableArray> tab_views_;
  base::scoped_nsobject<NSView> active_tab_view_;
  base::scoped_nsobject<NSView> tab0_;
  base::scoped_nsobject<NSView> tab1_;
};

// Test showing then hiding the sheet.
TEST_F(ConstrainedWindowSheetControllerTest, ShowHide) {
  EXPECT_FALSE([sheet_window_ isVisible]);
  [controller_ showSheet:sheet_ forParentView:active_tab_view_];
  EXPECT_TRUE([ConstrainedWindowSheetController controllerForSheet:sheet_]);
  EXPECT_TRUE([sheet_window_ isVisible]);

  [controller_ closeSheet:sheet_];
  EXPECT_FALSE([ConstrainedWindowSheetController controllerForSheet:sheet_]);
  EXPECT_FALSE([sheet_window_ isVisible]);
}

// Test that switching tabs correctly hides the inactive tab's sheet.
TEST_F(ConstrainedWindowSheetControllerTest, SwitchTabs) {
  [controller_ showSheet:sheet_ forParentView:active_tab_view_];

  EXPECT_TRUE([sheet_window_ isVisible]);
  EXPECT_EQ(1.0, [sheet_window_ alphaValue]);
  ActivateTabView([tab_views_ objectAtIndex:1]);
  EXPECT_TRUE([sheet_window_ isVisible]);
  EXPECT_EQ(0.0, [sheet_window_ alphaValue]);
  ActivateTabView([tab_views_ objectAtIndex:0]);
  EXPECT_TRUE([sheet_window_ isVisible]);
  EXPECT_EQ(1.0, [sheet_window_ alphaValue]);
}

// Test that adding a sheet to an inactive view doesn't show it.
TEST_F(ConstrainedWindowSheetControllerTest, AddToInactiveTab) {
  ActivateTabView(tab0_);
  [controller_ showSheet:sheet_ forParentView:tab1_];
  EXPECT_EQ(0.0, [sheet_window_ alphaValue]);

  ActivateTabView(tab1_);
  EXPECT_EQ(1.0, [sheet_window_ alphaValue]);
  VerifySheetXPosition([sheet_window_ frame], tab1_);
}

// Test that two parent windows with two sheet controllers don't conflict.
TEST_F(ConstrainedWindowSheetControllerTest, TwoParentWindows) {
  base::scoped_nsobject<NSWindow> parent_window2(
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 30, 30)
                                  styleMask:NSTitledWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  [parent_window2 setReleasedWhenClosed:NO];

  ConstrainedWindowSheetController* controller2 =
      [ConstrainedWindowSheetController
          controllerForParentWindow:parent_window2];
  EXPECT_TRUE(controller2);
  EXPECT_NSNE(controller_, controller2);

  [controller2 showSheet:sheet_ forParentView:[parent_window2 contentView]];
  EXPECT_NSEQ(controller2,
              [ConstrainedWindowSheetController controllerForSheet:sheet_]);

  [parent_window2 close];
}

// Test that using a top level parent view works.
TEST_F(ConstrainedWindowSheetControllerTest, TopLevelView) {
  NSView* parentView = [[test_window() contentView] superview];
  [controller_ parentViewDidBecomeActive:parentView];

  EXPECT_FALSE([sheet_window_ isVisible]);
  [controller_ showSheet:sheet_ forParentView:parentView];
  EXPECT_TRUE([ConstrainedWindowSheetController controllerForSheet:sheet_]);
  EXPECT_TRUE([sheet_window_ isVisible]);
  VerifySheetXPosition([sheet_window_ frame], parentView);
}

// Test that resizing sheet works.
TEST_F(ConstrainedWindowSheetControllerTest, Resize) {
  [controller_ showSheet:sheet_ forParentView:active_tab_view_];

  NSRect old_frame = [sheet_window_ frame];

  NSRect sheet_frame;
  sheet_frame.size = NSMakeSize(NSWidth(old_frame) + 100,
                                NSHeight(old_frame) + 50);
  sheet_frame.origin = [controller_ originForSheet:sheet_
                                    withWindowSize:sheet_frame.size];

  // Y pos should not have changed.
  EXPECT_EQ(NSMaxY(sheet_frame), NSMaxY(old_frame));

  // X pos should be centered on parent view.
  VerifySheetXPosition(sheet_frame, active_tab_view_);
}

// Test that resizing a hidden sheet works.
TEST_F(ConstrainedWindowSheetControllerTest, ResizeHiddenSheet) {
  [controller_ showSheet:sheet_ forParentView:tab0_];
  EXPECT_EQ(1.0, [sheet_window_ alphaValue]);
  ActivateTabView(tab1_);
  EXPECT_EQ(0.0, [sheet_window_ alphaValue]);

  NSRect old_frame = [sheet_window_ frame];
  NSRect new_inactive_frame = NSInsetRect(old_frame, -30, -40);
  [sheet_window_ setFrame:new_inactive_frame display:YES];

  ActivateTabView(tab0_);
  EXPECT_EQ(1.0, [sheet_window_ alphaValue]);

  NSRect new_active_frame = [sheet_window_ frame];
  EXPECT_EQ(NSWidth(new_inactive_frame), NSWidth(new_active_frame));
  EXPECT_EQ(NSHeight(new_inactive_frame), NSHeight(new_active_frame));
}

// Test resizing parent window keeps the sheet anchored.
TEST_F(ConstrainedWindowSheetControllerTest, ResizeParentWindow) {
  [controller_ showSheet:sheet_ forParentView:active_tab_view_];
  CGFloat sheet_offset =
      GetSheetYOffset([sheet_window_ frame], active_tab_view_);

  // Test 3x3 different parent window sizes.
  CGFloat insets[] = {-10, 0, 10};
  NSRect old_frame = [test_window() frame];

  for (size_t x = 0; x < sizeof(insets) / sizeof(CGFloat); x++) {
    for (size_t y = 0; y < sizeof(insets) / sizeof(CGFloat); y++) {
      NSRect resized_frame = NSInsetRect(old_frame, insets[x], insets[y]);
      [test_window() setFrame:resized_frame display:YES];
      NSRect sheet_frame = [sheet_window_ frame];

      // Y pos should track parent view's position.
      EXPECT_EQ(sheet_offset, GetSheetYOffset(sheet_frame, active_tab_view_));

      // X pos should be centered on parent view.
      VerifySheetXPosition(sheet_frame, active_tab_view_);
    }
  }
}

// Test system sheets.
TEST_F(ConstrainedWindowSheetControllerTest, SystemSheet) {
  base::scoped_nsobject<ConstrainedWindowSystemSheetTest> system_sheet(
      [[ConstrainedWindowSystemSheetTest alloc] init]);
  base::scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);
  [system_sheet setAlert:alert];

  EXPECT_FALSE([[alert window] isVisible]);
  [controller_ showSheet:system_sheet
                 forParentView:active_tab_view_];
  EXPECT_TRUE([[alert window] isVisible]);

  [controller_ closeSheet:system_sheet];
  EXPECT_FALSE([[alert window] isVisible]);
  EXPECT_EQ(kSystemSheetReturnCode, [system_sheet returnCode]);
}

// Test showing a system sheet on an inactive tab.
TEST_F(ConstrainedWindowSheetControllerTest, SystemSheetAddToInactiveTab) {
  base::scoped_nsobject<ConstrainedWindowSystemSheetTest> system_sheet(
      [[ConstrainedWindowSystemSheetTest alloc] init]);
  base::scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);
  [system_sheet setAlert:alert];

  EXPECT_FALSE([[alert window] isVisible]);
  [controller_ showSheet:system_sheet
                 forParentView:tab1_];
  EXPECT_FALSE([[alert window] isVisible]);

  ActivateTabView(tab1_);
  EXPECT_TRUE([[alert window] isVisible]);
  EXPECT_EQ(1.0, [[alert window] alphaValue]);

  [controller_ closeSheet:system_sheet];
}
