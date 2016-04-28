// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/win/app_list_win.h"

#include "chrome/browser/ui/app_list/app_list_positioner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace {

const int kScreenWidth = 800;
const int kScreenHeight = 600;

const int kWindowWidth = 100;
const int kWindowHeight = 200;

// Size of the normal (non-hidden) taskbar.
const int kTaskbarSize = 30;

// The distance the taskbar will appear from the edge of the screen.
const int kMinDistanceFromEdge = 3;

// Max distance the mouse can be from taskbar to count as "near" the taskbar.
const int kSnapDistance = 50;
// A cursor position that is within the taskbar. This must be < kTaskbarSize.
const int kCursorOnTaskbar = kTaskbarSize / 2;
// A cursor position that is within 50 pixels of the taskbar.
const int kCursorNearTaskbar = kTaskbarSize + kSnapDistance;
// A cursor position that is more than 50 pixels away from the taskbar.
const int kCursorAwayFromTaskbar = kCursorNearTaskbar + 1;

// A position for the center of the window that causes the window to overlap the
// edge of the screen. This must be < kWindowWidth / 2 and < kWindowHeight / 2.
const int kWindowNearEdge = kWindowWidth / 4;
// A position for the center of the window that places the window away from all
// edges of the screen. This must be > kWindowWidth / 2, > kWindowHeight / 2, <
// kScreenWidth - kWindowWidth / 2 and < kScreenHeight - kWindowHeight / 2.
const int kWindowAwayFromEdge = 158;

// Horizontal offset of the simulated Windows 8 split point.
const int kWin8SplitPoint = 200;

}  // namespace

class AppListWinUnitTest : public testing::Test {
 public:
  void SetUp() override {
    display_.set_bounds(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
    display_.set_work_area(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
    cursor_ = gfx::Point();
    taskbar_rect_ = gfx::Rect();
    center_window_ = false;
  }

  // Set the display work area.
  void SetWorkArea(int x, int y, int width, int height) {
    display_.set_work_area(gfx::Rect(x, y, width, height));
  }

  // Sets up the test environment with the taskbar along a given edge of the
  // work area.
  void PlaceTaskbar(AppListPositioner::ScreenEdge edge) {
    const gfx::Rect& work_area = display_.work_area();
    switch (edge) {
      case AppListPositioner::SCREEN_EDGE_UNKNOWN:
        taskbar_rect_ = gfx::Rect();
        break;
      case AppListPositioner::SCREEN_EDGE_LEFT:
        taskbar_rect_ = gfx::Rect(
            work_area.x(), work_area.y(), kTaskbarSize, work_area.height());
        break;
      case AppListPositioner::SCREEN_EDGE_RIGHT:
        taskbar_rect_ =
            gfx::Rect(work_area.x() + work_area.width() - kTaskbarSize,
                      work_area.y(),
                      kTaskbarSize,
                      work_area.height());
        break;
      case AppListPositioner::SCREEN_EDGE_TOP:
        taskbar_rect_ = gfx::Rect(
            work_area.x(), work_area.y(), work_area.width(), kTaskbarSize);
        break;
      case AppListPositioner::SCREEN_EDGE_BOTTOM:
        taskbar_rect_ =
            gfx::Rect(work_area.x(),
                      work_area.y() + work_area.height() - kTaskbarSize,
                      work_area.width(),
                      kTaskbarSize);
        break;
    }
  }

  // Set up the test mouse cursor in a given location.
  void PlaceCursor(int x, int y) {
    cursor_ = gfx::Point(x, y);
  }

  void EnableWindowCentering() {
    center_window_ = true;
  }

  gfx::Point DoFindAnchorPoint() const {
    return AppListWin::FindAnchorPoint(gfx::Size(kWindowWidth, kWindowHeight),
                                       display_,
                                       cursor_,
                                       taskbar_rect_,
                                       center_window_);
  }

 private:
  display::Display display_;
  gfx::Point cursor_;
  gfx::Rect taskbar_rect_;
  bool center_window_;
};

TEST_F(AppListWinUnitTest, FindAnchorPointNoTaskbar) {
  // Position the app list when there is no taskbar on the display.
  PlaceCursor(0, 0);
  // Expect the app list to be in the bottom-left corner.
  EXPECT_EQ(
      gfx::Point(kWindowWidth / 2 + kMinDistanceFromEdge,
                 kScreenHeight - kWindowHeight / 2 - kMinDistanceFromEdge),
      DoFindAnchorPoint());
}

TEST_F(AppListWinUnitTest, FindAnchorPointMouseOffTaskbar) {
  // Position the app list when the mouse is away from the taskbar.

  // Bottom taskbar. Expect the app list to be in the bottom-left corner.
  PlaceTaskbar(AppListPositioner::SCREEN_EDGE_BOTTOM);
  PlaceCursor(kWindowAwayFromEdge, kScreenHeight - kCursorAwayFromTaskbar);
  EXPECT_EQ(gfx::Point(kWindowWidth / 2 + kMinDistanceFromEdge,
                       kScreenHeight - kTaskbarSize - kWindowHeight / 2 -
                           kMinDistanceFromEdge),
            DoFindAnchorPoint());

  // Top taskbar. Expect the app list to be in the top-left corner.
  PlaceTaskbar(AppListPositioner::SCREEN_EDGE_TOP);
  PlaceCursor(kWindowAwayFromEdge, kCursorAwayFromTaskbar);
  EXPECT_EQ(gfx::Point(kWindowWidth / 2 + kMinDistanceFromEdge,
                       kTaskbarSize + kWindowHeight / 2 + kMinDistanceFromEdge),
            DoFindAnchorPoint());

  // Left taskbar. Expect the app list to be in the top-left corner.
  PlaceTaskbar(AppListPositioner::SCREEN_EDGE_LEFT);
  PlaceCursor(kCursorAwayFromTaskbar, kWindowAwayFromEdge);
  EXPECT_EQ(gfx::Point(kTaskbarSize + kWindowWidth / 2 + kMinDistanceFromEdge,
                       kWindowHeight / 2 + kMinDistanceFromEdge),
            DoFindAnchorPoint());

  // Right taskbar. Expect the app list to be in the top-right corner.
  PlaceTaskbar(AppListPositioner::SCREEN_EDGE_RIGHT);
  PlaceCursor(kScreenWidth - kCursorAwayFromTaskbar, kWindowAwayFromEdge);
  EXPECT_EQ(gfx::Point(kScreenWidth - kTaskbarSize - kWindowWidth / 2 -
                           kMinDistanceFromEdge,
                       kWindowHeight / 2 + kMinDistanceFromEdge),
            DoFindAnchorPoint());
}

TEST_F(AppListWinUnitTest, FindAnchorPointMouseOnTaskbar) {
  // Position the app list when the mouse is over the taskbar.

  // Bottom taskbar (mouse well within taskbar). Expect the app list to be at
  // the bottom centered on the mouse X coordinate.
  PlaceTaskbar(AppListPositioner::SCREEN_EDGE_BOTTOM);
  PlaceCursor(kWindowAwayFromEdge, kScreenHeight - kCursorOnTaskbar);
  EXPECT_EQ(gfx::Point(kWindowAwayFromEdge,
                       kScreenHeight - kTaskbarSize - kWindowHeight / 2 -
                           kMinDistanceFromEdge),
            DoFindAnchorPoint());

  // Bottom taskbar (outside the taskbar but still close enough).
  // Expect the app list to be at the bottom centered on the mouse X coordinate.
  PlaceTaskbar(AppListPositioner::SCREEN_EDGE_BOTTOM);
  PlaceCursor(kWindowAwayFromEdge, kScreenHeight - kCursorNearTaskbar);
  EXPECT_EQ(gfx::Point(kWindowAwayFromEdge,
                       kScreenHeight - kTaskbarSize - kWindowHeight / 2 -
                           kMinDistanceFromEdge),
            DoFindAnchorPoint());

  // Top taskbar. Expect the app list to be at the top centered on the
  // mouse X coordinate.
  PlaceTaskbar(AppListPositioner::SCREEN_EDGE_TOP);
  PlaceCursor(kWindowAwayFromEdge, kCursorNearTaskbar);
  EXPECT_EQ(gfx::Point(kWindowAwayFromEdge,
                       kTaskbarSize + kWindowHeight / 2 + kMinDistanceFromEdge),
            DoFindAnchorPoint());

  // Left taskbar. Expect the app list to be at the left centered on the
  // mouse Y coordinate.
  PlaceTaskbar(AppListPositioner::SCREEN_EDGE_LEFT);
  PlaceCursor(kCursorNearTaskbar, kWindowAwayFromEdge);
  EXPECT_EQ(gfx::Point(kTaskbarSize + kWindowWidth / 2 + kMinDistanceFromEdge,
                       kWindowAwayFromEdge),
            DoFindAnchorPoint());

  // Right taskbar. Expect the app list to be at the right centered on the
  // mouse Y coordinate.
  PlaceTaskbar(AppListPositioner::SCREEN_EDGE_RIGHT);
  PlaceCursor(kScreenWidth - kCursorNearTaskbar, kWindowAwayFromEdge);
  EXPECT_EQ(gfx::Point(kScreenWidth - kTaskbarSize - kWindowWidth / 2 -
                           kMinDistanceFromEdge,
                       kWindowAwayFromEdge),
            DoFindAnchorPoint());

  // Bottom taskbar. Mouse near left edge. App list must not go off screen.
  PlaceTaskbar(AppListPositioner::SCREEN_EDGE_BOTTOM);
  PlaceCursor(kWindowNearEdge, kScreenHeight - kCursorOnTaskbar);
  EXPECT_EQ(gfx::Point(kWindowWidth / 2 + kMinDistanceFromEdge,
                       kScreenHeight - kTaskbarSize - kWindowHeight / 2 -
                           kMinDistanceFromEdge),
            DoFindAnchorPoint());

  // Bottom taskbar. Mouse near right edge. App list must not go off screen.
  PlaceTaskbar(AppListPositioner::SCREEN_EDGE_BOTTOM);
  PlaceCursor(kScreenWidth - kWindowNearEdge, kScreenHeight - kCursorOnTaskbar);
  EXPECT_EQ(gfx::Point(kScreenWidth - kWindowWidth / 2 - kMinDistanceFromEdge,
                       kScreenHeight - kTaskbarSize - kWindowHeight / 2 -
                           kMinDistanceFromEdge),
            DoFindAnchorPoint());
}

TEST_F(AppListWinUnitTest, FindAnchorPointWin8SplitScreen) {
  // Make the work area smaller than the screen, as you would get in Windows 8
  // when there is a split screen and the Desktop is in one side of the screen.
  SetWorkArea(
      kWin8SplitPoint, 0, kScreenWidth - kWin8SplitPoint, kScreenHeight);

  // No taskbar. Expect the app list to be in the bottom-left corner of the work
  // area.
  EXPECT_EQ(
      gfx::Point(kWin8SplitPoint + kWindowWidth / 2 + kMinDistanceFromEdge,
                 kScreenHeight - kWindowHeight / 2 - kMinDistanceFromEdge),
      DoFindAnchorPoint());

  // Bottom taskbar (mouse off-screen to the left). Expect the app list to be
  // in the bottom-left corner of the work area.
  PlaceTaskbar(AppListPositioner::SCREEN_EDGE_BOTTOM);
  PlaceCursor(kWindowAwayFromEdge, kScreenHeight - kCursorOnTaskbar);
  EXPECT_EQ(
      gfx::Point(kWin8SplitPoint + kWindowWidth / 2 + kMinDistanceFromEdge,
                 kScreenHeight - kTaskbarSize - kWindowHeight / 2 -
                     kMinDistanceFromEdge),
      DoFindAnchorPoint());
}

TEST_F(AppListWinUnitTest, FindAnchorPointCentered) {
  // Cursor on the top taskbar; enable centered app list mode.
  PlaceTaskbar(AppListPositioner::SCREEN_EDGE_TOP);
  PlaceCursor(0, 0);
  EnableWindowCentering();
  // Expect the app list to be in the center of the screen (ignore the cursor).
  EXPECT_EQ(gfx::Point(kScreenWidth / 2, kScreenHeight / 2),
            DoFindAnchorPoint());
}
