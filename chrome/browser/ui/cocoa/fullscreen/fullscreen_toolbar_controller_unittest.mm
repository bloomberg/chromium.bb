// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_menubar_tracker.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_animation_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_mouse_tracker.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_visibility_lock_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/cocoa/appkit_utils.h"

//////////////////////////////////////////////////////////////////
// MockFullscreenToolbarMouseTracker
// Mocks the mouse interactions with the toolbar.

@interface MockFullscreenToolbarMouseTracker : FullscreenToolbarMouseTracker

@property(nonatomic, assign) BOOL mouseInside;

// Overridden to prevent a tracking area from being created.
- (void)updateTrackingArea;

- (BOOL)mouseInsideTrackingArea;

@end

@implementation MockFullscreenToolbarMouseTracker

@synthesize mouseInside = mouseInside_;

- (void)updateTrackingArea {
}

- (BOOL)mouseInsideTrackingArea {
  return mouseInside_;
}

@end

//////////////////////////////////////////////////////////////////
// MockFullscreenMenubarTracker
// Mocks the state of the menubar.

@interface MockFullscreenMenubarTracker : FullscreenMenubarTracker {
  CGFloat menubarFraction_;
  FullscreenMenubarState menubarState_;
}

- (CGFloat)menubarFraction;
- (FullscreenMenubarState)state;
- (void)setMenubarProgress:(CGFloat)progress;

@end

@implementation MockFullscreenMenubarTracker

- (CGFloat)menubarFraction {
  return menubarFraction_;
}

- (FullscreenMenubarState)state {
  return menubarState_;
}

- (void)setMenubarProgress:(CGFloat)progress {
  if (ui::IsCGFloatEqual(progress, 1.0))
    menubarState_ = FullscreenMenubarState::SHOWN;
  else if (ui::IsCGFloatEqual(progress, 0.0))
    menubarState_ = FullscreenMenubarState::HIDDEN;
  else if (progress < menubarFraction_)
    menubarState_ = FullscreenMenubarState::HIDING;
  else if (progress > menubarFraction_)
    menubarState_ = FullscreenMenubarState::SHOWING;
  menubarFraction_ = progress;
}

@end

namespace {

class FullscreenToolbarControllerTest : public testing::Test {
 public:
  FullscreenToolbarControllerTest() {}
  void SetUp() override {
    BOOL yes = YES;
    BOOL no = NO;

    bwc_ = [OCMockObject mockForClass:[BrowserWindowController class]];
    [[[bwc_ stub] andReturnValue:OCMOCK_VALUE(yes)]
        isKindOfClass:[BrowserWindowController class]];
    [[[bwc_ stub] andReturnValue:OCMOCK_VALUE(yes)] isInAppKitFullscreen];
    [[[bwc_ stub] andReturnValue:OCMOCK_VALUE(no)] isInImmersiveFullscreen];
    [[bwc_ stub] layoutSubviews];

    controller_.reset(
        [[FullscreenToolbarController alloc] initWithBrowserController:bwc_]);
    SetToolbarStyle(FullscreenToolbarStyle::TOOLBAR_HIDDEN);

    menubar_tracker_.reset([[MockFullscreenMenubarTracker alloc]
        initWithFullscreenToolbarController:nil]);
    [menubar_tracker_ setMenubarProgress:0.0];
    [controller_ setMenubarTracker:menubar_tracker_];

    mouse_tracker_.reset([[MockFullscreenToolbarMouseTracker alloc] init]);
    [controller_ setMouseTracker:mouse_tracker_];

    [controller_ animationController]->SetAnimationDuration(0.0);

    [controller_ setTestFullscreenMode:YES];
  }

  void TearDown() override { [controller_ setTestFullscreenMode:NO]; }

  void SetToolbarStyle(FullscreenToolbarStyle style) {
    [controller_ setToolbarStyle:style];
  }

  void CheckLayout(CGFloat toolbarFraction, CGFloat menubarFraction) {
    FullscreenToolbarLayout layout = [controller_ computeLayout];
    EXPECT_EQ(toolbarFraction, layout.toolbarFraction);
    EXPECT_EQ(menubarFraction * [controller_ toolbarVerticalOffset],
              layout.menubarOffset);
  }

  // A mock BrowserWindowController object.
  id bwc_;

  // The FullscreenToolbarController object being tested.
  base::scoped_nsobject<FullscreenToolbarController> controller_;

  // Mocks the state of the menubar.
  base::scoped_nsobject<MockFullscreenMenubarTracker> menubar_tracker_;

  // Mocks the mouse interactions with the toolbar.
  base::scoped_nsobject<MockFullscreenToolbarMouseTracker> mouse_tracker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FullscreenToolbarControllerTest);
};

// Tests the toolbar fraction for the TOOLBAR_NONE and TOOLBAR_PRESENT
// styles.
TEST_F(FullscreenToolbarControllerTest, TestPresentAndNoneToolbarStyle) {
  CheckLayout(0, 0);

  [controller_ setToolbarStyle:FullscreenToolbarStyle::TOOLBAR_NONE];
  CheckLayout(0, 0);

  [controller_ setToolbarStyle:FullscreenToolbarStyle::TOOLBAR_PRESENT];
  CheckLayout(1, 0);
}

// Basic test that checks if the toolbar fraction for different menubar values.
// This test simulates the showing and hiding the menubar.
TEST_F(FullscreenToolbarControllerTest, TestHiddenToolbarWithMenubar) {
  CheckLayout(0, 0);

  [menubar_tracker_ setMenubarProgress:0.5];
  CheckLayout(0.5, 0.5);

  [menubar_tracker_ setMenubarProgress:1];
  CheckLayout(1, 1);

  [menubar_tracker_ setMenubarProgress:0.5];
  CheckLayout(0.5, 0.5);

  [menubar_tracker_ setMenubarProgress:0];
  CheckLayout(0, 0);
}

// Test that checks the visibility lock functions and the toolbar fraction.
TEST_F(FullscreenToolbarControllerTest, TestHiddenToolbarWithVisibilityLocks) {
  FullscreenToolbarVisibilityLockController* locks =
      [controller_ visibilityLockController];
  base::scoped_nsobject<NSObject> owner([[NSObject alloc] init]);
  base::scoped_nsobject<NSObject> alt_owner([[NSObject alloc] init]);

  [menubar_tracker_ setMenubarProgress:0];
  CheckLayout(0, 0);

  // Lock the toolbar visibility. Toolbar should be fully visible.
  [locks lockToolbarVisibilityForOwner:owner.get() withAnimation:NO];
  EXPECT_TRUE([locks isToolbarVisibilityLocked]);
  EXPECT_TRUE([locks isToolbarVisibilityLockedForOwner:owner.get()]);
  EXPECT_TRUE(![locks isToolbarVisibilityLockedForOwner:alt_owner.get()]);
  CheckLayout(1, 0);

  // Show the menubar.
  [menubar_tracker_ setMenubarProgress:1];
  CheckLayout(1, 1);

  // Hide the menubar. The toolbar should still be fully visible.
  [menubar_tracker_ setMenubarProgress:0.5];
  CheckLayout(1, 0.5);
  [menubar_tracker_ setMenubarProgress:0];
  CheckLayout(1, 0);

  // Release the lock. Toolbar should now be hidden.
  [locks releaseToolbarVisibilityForOwner:owner.get() withAnimation:NO];
  EXPECT_TRUE(![locks isToolbarVisibilityLocked]);
  EXPECT_TRUE(![locks isToolbarVisibilityLockedForOwner:owner.get()]);
  CheckLayout(0, 0);

  // Lock and release the toolbar visibility with multiple owners.
  [locks lockToolbarVisibilityForOwner:owner.get() withAnimation:NO];
  [locks lockToolbarVisibilityForOwner:alt_owner.get() withAnimation:NO];
  EXPECT_TRUE([locks isToolbarVisibilityLocked]);
  EXPECT_TRUE([locks isToolbarVisibilityLockedForOwner:owner.get()]);
  EXPECT_TRUE([locks isToolbarVisibilityLockedForOwner:alt_owner.get()]);
  CheckLayout(1, 0);

  [locks releaseToolbarVisibilityForOwner:owner.get() withAnimation:NO];
  EXPECT_TRUE([locks isToolbarVisibilityLocked]);
  EXPECT_TRUE(![locks isToolbarVisibilityLockedForOwner:owner.get()]);
  EXPECT_TRUE([locks isToolbarVisibilityLockedForOwner:alt_owner.get()]);
  CheckLayout(1, 0);

  [locks releaseToolbarVisibilityForOwner:alt_owner.get() withAnimation:NO];
  EXPECT_TRUE(![locks isToolbarVisibilityLocked]);
  EXPECT_TRUE(![locks isToolbarVisibilityLockedForOwner:alt_owner.get()]);
  CheckLayout(0, 0);
}

// Basic test that checks the toolbar fraction for different mouse tracking
// values.
TEST_F(FullscreenToolbarControllerTest, TestHiddenToolbarWithMouseTracking) {
  CheckLayout(0, 0);

  [mouse_tracker_ setMouseInside:YES];
  CheckLayout(1, 0);

  [menubar_tracker_ setMenubarProgress:1];
  CheckLayout(1, 1);

  [menubar_tracker_ setMenubarProgress:0];
  CheckLayout(1, 0);

  [mouse_tracker_ setMouseInside:NO];
  CheckLayout(0, 0);
}

// Test that checks the toolbar fraction with mouse tracking, menubar fraction,
// and visibility locks.
TEST_F(FullscreenToolbarControllerTest, TestHiddenToolbarWithMultipleFactors) {
  FullscreenToolbarVisibilityLockController* locks =
      [controller_ visibilityLockController];
  base::scoped_nsobject<NSObject> owner([[NSObject alloc] init]);
  CheckLayout(0, 0);

  // Toolbar should be shown with the menubar.
  [menubar_tracker_ setMenubarProgress:1];
  CheckLayout(1, 1);

  // Move the mouse to the toolbar and start hiding the menubar. Toolbar
  // should be fully visible.
  [mouse_tracker_ setMouseInside:YES];
  CheckLayout(1, 1);
  [menubar_tracker_ setMenubarProgress:0.5];
  CheckLayout(1, 0.5);

  // Lock the toolbar's visibility.
  [locks lockToolbarVisibilityForOwner:owner.get() withAnimation:NO];
  CheckLayout(1, 0.5);

  // Hide the menubar. Toolbar should be fully visible.
  [menubar_tracker_ setMenubarProgress:0];
  CheckLayout(1, 0);

  // Lock the toolbar's visibility. Toolbar should be fully visible.
  [locks releaseToolbarVisibilityForOwner:owner.get() withAnimation:NO];
  CheckLayout(1, 0);

  // Move the mouse away from the toolbar. Toolbar should hide.
  [mouse_tracker_ setMouseInside:NO];
  CheckLayout(0, 0);

  // Lock the toolbar and move the mouse to it.
  [locks lockToolbarVisibilityForOwner:owner.get() withAnimation:NO];
  [mouse_tracker_ setMouseInside:YES];
  CheckLayout(1, 0);

  // Move the mouse away from the toolbar. Toolbar should be fully visible.
  [mouse_tracker_ setMouseInside:NO];
  CheckLayout(1, 0);

  // Release the toolbar. Toolbar should be hidden.
  [locks releaseToolbarVisibilityForOwner:owner.get() withAnimation:NO];
}

}  // namespace
