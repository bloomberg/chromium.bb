// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/app_list/app_list_service_mac.h"

#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace {

const int kScreenWidth = 800;
const int kScreenHeight = 600;

const int kWindowWidth = 100;
const int kWindowHeight = 200;

// Size of the menu bar along the top of the screen.
const int kMenuBarSize = 22;
// Size of the normal (non-hidden) dock.
const int kDockSize = 30;
// Size of the hidden dock.
const int kHiddenDockSize = 4;

// Distance to move during the opening animation.
const int kAnimationOffset = 20;

// The assumed size of the hidden dock.
const int kExtraDistance = 50;

// A cursor position that is within the dock. This must be < kDockSize.
const int kCursorOnDock = kDockSize / 2;
// A cursor position that is within kWindowWidth pixels of the dock.
const int kCursorNearDockX = kDockSize + kWindowWidth;
// A cursor position that is more than kWindowWidth pixels away from the dock.
const int kCursorAwayFromDockX = kCursorNearDockX + 1;
// A cursor position that is within kWindowHeight pixels of the dock.
const int kCursorNearDockY = kDockSize + kWindowHeight;
// A cursor position that is more than kWindowHeight pixels away from the dock.
const int kCursorAwayFromDockY = kCursorNearDockY + 1;

// A position for the center of the window that causes the window to overlap the
// edge of the screen. This must be < kWindowWidth / 2 and < kWindowHeight / 2.
const int kWindowNearEdge = kWindowWidth / 4;
// A position for the center of the window that places the window away from all
// edges of the screen. This must be > kWindowWidth / 2, > kWindowHeight / 2, <
// kScreenWidth - kWindowWidth / 2 and < kScreenHeight - kWindowHeight / 2.
const int kWindowAwayFromEdge = 158;

enum DockLocation {
  DOCK_LOCATION_BOTTOM,
  DOCK_LOCATION_LEFT,
  DOCK_LOCATION_RIGHT,
};

// Returns |point| offset by (|x_offset|, |y_offset|).
NSPoint OffsetPoint(const NSPoint& point, CGFloat x_offset, CGFloat y_offset) {
  return NSMakePoint(point.x + x_offset, point.y + y_offset);
}

}  // namespace

class AppListServiceMacUnitTest : public testing::Test {
 public:
  void SetUp() override {
    display_.set_bounds(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
    display_.set_work_area(
        gfx::Rect(0, kMenuBarSize, kScreenWidth, kScreenHeight - kMenuBarSize));
    window_size_ = gfx::Size(kWindowWidth, kWindowHeight);
    cursor_is_visible_ = true;
    cursor_ = gfx::Point();
  }

  // Sets up the test environment with the dock in a given location.
  void PlaceDock(DockLocation location, bool hidden) {
    int dock_size = hidden ? kHiddenDockSize : kDockSize;
    switch (location) {
      case DOCK_LOCATION_BOTTOM:
        display_.set_work_area(
            gfx::Rect(0,
                      kMenuBarSize,
                      kScreenWidth,
                      kScreenHeight - kMenuBarSize - dock_size));
        break;
      case DOCK_LOCATION_LEFT:
        display_.set_work_area(gfx::Rect(dock_size,
                                         kMenuBarSize,
                                         kScreenWidth - dock_size,
                                         kScreenHeight - kMenuBarSize));
        break;
      case DOCK_LOCATION_RIGHT:
        display_.set_work_area(gfx::Rect(0,
                                         kMenuBarSize,
                                         kScreenWidth - dock_size,
                                         kScreenHeight - kMenuBarSize));
        break;
    }
  }

  // Set whether the test mouse cursor is visible.
  void SetCursorVisible(bool visible) {
    cursor_is_visible_ = visible;
  }

  // Set up the test mouse cursor in a given location.
  void PlaceCursor(int x, int y) {
    cursor_ = gfx::Point(x, y);
  }

  void DoFindAnchorPoint(NSPoint* target_origin,
                         NSPoint* start_origin) {
    AppListServiceMac::FindAnchorPoint(window_size_,
                                       display_,
                                       kScreenHeight,
                                       cursor_is_visible_,
                                       cursor_,
                                       target_origin,
                                       start_origin);
  }

 private:
  display::Display display_;
  gfx::Size window_size_;
  bool cursor_is_visible_;
  gfx::Point cursor_;
};

// Tests positioning the app list when there is no visible mouse cursor.
TEST_F(AppListServiceMacUnitTest, FindAnchorPointNoCursor) {
  SetCursorVisible(false);
  PlaceCursor(0, 0);
  NSPoint target_origin;
  NSPoint start_origin;

  // Bottom dock. Expect the app list to be centered on the dock.
  PlaceDock(DOCK_LOCATION_BOTTOM, false);
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(NSMakePoint(kScreenWidth / 2 - kWindowWidth / 2, kDockSize),
              target_origin);
  EXPECT_NSEQ(OffsetPoint(target_origin, 0, -kAnimationOffset), start_origin);

  // Left dock. Expect the app list to be centered on the dock.
  PlaceDock(DOCK_LOCATION_LEFT, false);
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(NSMakePoint(kDockSize, (kScreenHeight - kMenuBarSize) / 2 -
                                         kWindowHeight / 2),
              target_origin);
  EXPECT_NSEQ(OffsetPoint(target_origin, -kAnimationOffset, 0), start_origin);

  // Right dock. Expect the app list to be centered on the dock.
  PlaceDock(DOCK_LOCATION_RIGHT, false);
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(
      NSMakePoint(kScreenWidth - kDockSize - kWindowWidth,
                  (kScreenHeight - kMenuBarSize) / 2 - kWindowHeight / 2),
      target_origin);
  EXPECT_NSEQ(OffsetPoint(target_origin, kAnimationOffset, 0), start_origin);
}

// Tests positioning the app list when there is no dock on the display.
TEST_F(AppListServiceMacUnitTest, FindAnchorPointNoDock) {
  SetCursorVisible(true);
  PlaceCursor(0, 0);
  NSPoint target_origin;
  NSPoint start_origin;

  // Expect the app list to be in the bottom-left corner.
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(NSMakePoint(0, 0), target_origin);
  EXPECT_NSEQ(target_origin, start_origin);

  // No mouse cursor. This should have no effect.
  SetCursorVisible(false);
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(NSMakePoint(0, 0), target_origin);
  EXPECT_NSEQ(target_origin, start_origin);
}

// Tests positioning the app list when the mouse is away from the dock.
TEST_F(AppListServiceMacUnitTest, FindAnchorPointMouseOffDock) {
  SetCursorVisible(true);
  NSPoint target_origin;
  NSPoint start_origin;

  // Bottom dock. Expect the app list to be centered on the dock.
  PlaceDock(DOCK_LOCATION_BOTTOM, false);
  PlaceCursor(kWindowAwayFromEdge, kScreenHeight - kCursorAwayFromDockY);
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(NSMakePoint(kScreenWidth / 2 - kWindowWidth / 2, kDockSize),
              target_origin);
  EXPECT_NSEQ(OffsetPoint(target_origin, 0, -kAnimationOffset), start_origin);

  // Left dock. Expect the app list to be centered on the dock.
  PlaceDock(DOCK_LOCATION_LEFT, false);
  PlaceCursor(kCursorAwayFromDockX, kWindowAwayFromEdge);
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(NSMakePoint(kDockSize, (kScreenHeight - kMenuBarSize) / 2 -
                                         kWindowHeight / 2),
              target_origin);
  EXPECT_NSEQ(OffsetPoint(target_origin, -kAnimationOffset, 0), start_origin);

  // Right dock. Expect the app list to be centered on the dock.
  PlaceDock(DOCK_LOCATION_RIGHT, false);
  PlaceCursor(kScreenWidth - kCursorAwayFromDockX, kWindowAwayFromEdge);
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(
      NSMakePoint(kScreenWidth - kDockSize - kWindowWidth,
                  (kScreenHeight - kMenuBarSize) / 2 - kWindowHeight / 2),
      target_origin);
  EXPECT_NSEQ(OffsetPoint(target_origin, kAnimationOffset, 0), start_origin);
}

// Tests positioning the app list when the dock is hidden.
TEST_F(AppListServiceMacUnitTest, FindAnchorPointHiddenDock) {
  SetCursorVisible(true);
  NSPoint target_origin;
  NSPoint start_origin;

  // Bottom dock. Expect the app list to be kExtraDistance pixels from the dock
  // centered on the mouse X coordinate.
  PlaceDock(DOCK_LOCATION_BOTTOM, true);
  PlaceCursor(kWindowAwayFromEdge, kScreenHeight - kCursorAwayFromDockY);
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(NSMakePoint(kWindowAwayFromEdge - kWindowWidth / 2,
                          kHiddenDockSize + kExtraDistance),
              target_origin);
  EXPECT_NSEQ(OffsetPoint(target_origin, 0, -kAnimationOffset), start_origin);

  // Left dock. Expect the app list to be kExtraDistance pixels from the dock
  // centered on the mouse Y coordinate.
  PlaceDock(DOCK_LOCATION_LEFT, true);
  PlaceCursor(kCursorAwayFromDockX, kWindowAwayFromEdge);
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(
      NSMakePoint(kHiddenDockSize + kExtraDistance,
                  kScreenHeight - kWindowAwayFromEdge - kWindowHeight / 2),
      target_origin);
  EXPECT_NSEQ(OffsetPoint(target_origin, -kAnimationOffset, 0), start_origin);

  // Right dock. Expect the app list to be kExtraDistance pixels from the dock
  // centered on the mouse Y coordinate.
  PlaceDock(DOCK_LOCATION_RIGHT, true);
  PlaceCursor(kScreenWidth - kCursorAwayFromDockX, kWindowAwayFromEdge);
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(
      NSMakePoint(
          kScreenWidth - kHiddenDockSize - kWindowWidth - kExtraDistance,
          kScreenHeight - kWindowAwayFromEdge - kWindowHeight / 2),
      target_origin);
  EXPECT_NSEQ(OffsetPoint(target_origin, kAnimationOffset, 0), start_origin);
}

// Tests positioning the app list when the mouse is over the dock.
TEST_F(AppListServiceMacUnitTest, FindAnchorPointMouseOnDock) {
  SetCursorVisible(true);
  NSPoint target_origin;
  NSPoint start_origin;

  // Bottom dock (mouse well within dock). Expect the app list to be at the
  // bottom centered on the mouse X coordinate.
  PlaceDock(DOCK_LOCATION_BOTTOM, false);
  PlaceCursor(kWindowAwayFromEdge, kScreenHeight - kCursorOnDock);
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(NSMakePoint(kWindowAwayFromEdge - kWindowWidth / 2, kDockSize),
              target_origin);
  EXPECT_NSEQ(OffsetPoint(target_origin, 0, -kAnimationOffset), start_origin);

  // Bottom dock (outside the dock but still close enough). Expect the app list
  // to be at the bottom centered on the mouse X coordinate.
  PlaceDock(DOCK_LOCATION_BOTTOM, false);
  PlaceCursor(kWindowAwayFromEdge, kScreenHeight - kCursorNearDockY);
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(NSMakePoint(kWindowAwayFromEdge - kWindowWidth / 2, kDockSize),
              target_origin);
  EXPECT_NSEQ(OffsetPoint(target_origin, 0, -kAnimationOffset), start_origin);

  // Left dock. Expect the app list to be at the left centered on the mouse Y
  // coordinate.
  PlaceDock(DOCK_LOCATION_LEFT, false);
  PlaceCursor(kCursorNearDockX, kWindowAwayFromEdge);
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(NSMakePoint(kDockSize, kScreenHeight - kWindowAwayFromEdge -
                                         kWindowHeight / 2),
              target_origin);
  EXPECT_NSEQ(OffsetPoint(target_origin, -kAnimationOffset, 0), start_origin);

  // Right dock. Expect the app list to be at the right centered on the mouse Y
  // coordinate.
  PlaceDock(DOCK_LOCATION_RIGHT, false);
  PlaceCursor(kScreenWidth - kCursorNearDockX, kWindowAwayFromEdge);
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(
      NSMakePoint(kScreenWidth - kDockSize - kWindowWidth,
                  kScreenHeight - kWindowAwayFromEdge - kWindowHeight / 2),
      target_origin);
  EXPECT_NSEQ(OffsetPoint(target_origin, kAnimationOffset, 0), start_origin);

  // Bottom dock. Mouse near left edge. App list must not go off screen.
  PlaceDock(DOCK_LOCATION_BOTTOM, false);
  PlaceCursor(kWindowNearEdge, kScreenHeight - kCursorOnDock);
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(NSMakePoint(0, kDockSize), target_origin);
  EXPECT_NSEQ(OffsetPoint(target_origin, 0, -kAnimationOffset), start_origin);

  // Bottom dock. Mouse near right edge. App list must not go off screen.
  PlaceDock(DOCK_LOCATION_BOTTOM, false);
  PlaceCursor(kScreenWidth - kWindowNearEdge, kScreenHeight - kCursorOnDock);
  DoFindAnchorPoint(&target_origin, &start_origin);
  EXPECT_NSEQ(NSMakePoint(kScreenWidth - kWindowWidth, kDockSize),
              target_origin);
  EXPECT_NSEQ(OffsetPoint(target_origin, 0, -kAnimationOffset), start_origin);
}
