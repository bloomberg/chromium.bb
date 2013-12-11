// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/linux/app_list_linux.h"

#include "chrome/browser/ui/app_list/app_list_positioner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace {

const int kScreenWidth = 800;
const int kScreenHeight = 600;

const int kWindowWidth = 100;
const int kWindowHeight = 200;

// Size of the menu bar along the top of the screen.
const int kMenuBarSize = 22;
// Size of the normal (non-hidden) shelf.
const int kShelfSize = 30;

// A cursor position that is within the shelf. This must be < kShelfSize.
const int kCursorOnShelf = kShelfSize / 2;
// A cursor position that is within kWindowWidth pixels of the shelf.
const int kCursorNearShelfX = kShelfSize + kWindowWidth;
// A cursor position that is more than kWindowWidth pixels away from the shelf.
const int kCursorAwayFromShelfX = kCursorNearShelfX + 1;
// A cursor position that is within kWindowHeight pixels of the shelf.
const int kCursorNearShelfY = kShelfSize + kWindowHeight;
// A cursor position that is more than kWindowHeight pixels away from the shelf.
const int kCursorAwayFromShelfY = kCursorNearShelfY + 1;

// A position for the center of the window that causes the window to overlap the
// edge of the screen. This must be < kWindowWidth / 2 and < kWindowHeight / 2.
const int kWindowNearEdge = kWindowWidth / 4;
// A position for the center of the window that places the window away from all
// edges of the screen. This must be > kWindowWidth / 2, > kWindowHeight / 2, <
// kScreenWidth - kWindowWidth / 2 and < kScreenHeight - kWindowHeight / 2.
const int kWindowAwayFromEdge = 158;

}  // namespace

class AppListLinuxUnitTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    display_.set_bounds(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
    display_.set_work_area(
        gfx::Rect(0, kMenuBarSize, kScreenWidth, kScreenHeight - kMenuBarSize));
    cursor_ = gfx::Point();
    shelf_edge_ = AppListPositioner::SCREEN_EDGE_UNKNOWN;
  }

  // Set the display work area.
  void SetWorkArea(int x, int y, int width, int height) {
    display_.set_work_area(gfx::Rect(x, y, width, height));
  }

  // Sets up the test environment with the shelf along a given edge of the
  // work area.
  void PlaceShelf(AppListPositioner::ScreenEdge edge) {
    shelf_edge_ = edge;
    switch (edge) {
      case AppListPositioner::SCREEN_EDGE_LEFT:
        display_.set_work_area(gfx::Rect(kShelfSize,
                                         kMenuBarSize,
                                         kScreenWidth - kShelfSize,
                                         kScreenHeight - kMenuBarSize));
        break;
      case AppListPositioner::SCREEN_EDGE_RIGHT:
        display_.set_work_area(gfx::Rect(0,
                                         kMenuBarSize,
                                         kScreenWidth - kShelfSize,
                                         kScreenHeight - kMenuBarSize));
        break;
      case AppListPositioner::SCREEN_EDGE_TOP:
        display_.set_work_area(
            gfx::Rect(0,
                      kMenuBarSize + kShelfSize,
                      kScreenWidth,
                      kScreenHeight - kMenuBarSize - kShelfSize));
        break;
      case AppListPositioner::SCREEN_EDGE_BOTTOM:
        display_.set_work_area(
            gfx::Rect(0,
                      kMenuBarSize,
                      kScreenWidth,
                      kScreenHeight - kMenuBarSize - kShelfSize));
        break;
      case AppListPositioner::SCREEN_EDGE_UNKNOWN:
        NOTREACHED();
        break;
    }
  }

  // Set up the test mouse cursor in a given location.
  void PlaceCursor(int x, int y) {
    cursor_ = gfx::Point(x, y);
  }

  gfx::Point DoFindAnchorPoint() const {
    return AppListLinux::FindAnchorPoint(gfx::Size(kWindowWidth, kWindowHeight),
                                         display_,
                                         cursor_,
                                         shelf_edge_);
  }

 private:
  gfx::Display display_;
  gfx::Point cursor_;
  AppListPositioner::ScreenEdge shelf_edge_;
};

TEST_F(AppListLinuxUnitTest, FindAnchorPointNoShelf) {
  // Position the app list when there is no shelf on the display.
  PlaceCursor(0, 0);
  // Expect the app list to be in the top-left corner.
  EXPECT_EQ(gfx::Point(kWindowWidth / 2, kMenuBarSize + kWindowHeight / 2),
            DoFindAnchorPoint());
}

TEST_F(AppListLinuxUnitTest, FindAnchorPointMouseOffShelf) {
  // Position the app list when the mouse is away from the shelf.

  // Bottom shelf. Expect the app list to be in the bottom-left corner.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_BOTTOM);
  PlaceCursor(kWindowAwayFromEdge, kScreenHeight - kCursorAwayFromShelfY);
  EXPECT_EQ(gfx::Point(kWindowWidth / 2,
                       kScreenHeight - kShelfSize - kWindowHeight / 2),
            DoFindAnchorPoint());

  // Top shelf. Expect the app list to be in the top-left corner.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_TOP);
  PlaceCursor(kWindowAwayFromEdge, kMenuBarSize + kCursorAwayFromShelfY);
  EXPECT_EQ(gfx::Point(kWindowWidth / 2,
                       kMenuBarSize + kShelfSize + kWindowHeight / 2),
            DoFindAnchorPoint());

  // Left shelf. Expect the app list to be in the top-left corner.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_LEFT);
  PlaceCursor(kCursorAwayFromShelfX, kWindowAwayFromEdge);
  EXPECT_EQ(gfx::Point(kShelfSize + kWindowWidth / 2,
                       kMenuBarSize + kWindowHeight / 2),
            DoFindAnchorPoint());

  // Right shelf. Expect the app list to be in the top-right corner.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_RIGHT);
  PlaceCursor(kScreenWidth - kCursorAwayFromShelfX, kWindowAwayFromEdge);
  EXPECT_EQ(gfx::Point(kScreenWidth - kShelfSize - kWindowWidth / 2,
                       kMenuBarSize + kWindowHeight / 2),
            DoFindAnchorPoint());
}

TEST_F(AppListLinuxUnitTest, FindAnchorPointMouseOnShelf) {
  // Position the app list when the mouse is over the shelf.

  // Bottom shelf (mouse well within shelf). Expect the app list to be at
  // the bottom centered on the mouse X coordinate.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_BOTTOM);
  PlaceCursor(kWindowAwayFromEdge, kScreenHeight - kCursorOnShelf);
  EXPECT_EQ(gfx::Point(kWindowAwayFromEdge,
                       kScreenHeight - kShelfSize - kWindowHeight / 2),
            DoFindAnchorPoint());

  // Bottom shelf (outside the shelf but still close enough).
  // Expect the app list to be at the bottom centered on the mouse X coordinate.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_BOTTOM);
  PlaceCursor(kWindowAwayFromEdge, kScreenHeight - kCursorNearShelfY);
  EXPECT_EQ(gfx::Point(kWindowAwayFromEdge,
                       kScreenHeight - kShelfSize - kWindowHeight / 2),
            DoFindAnchorPoint());

  // Top shelf. Expect the app list to be at the top centered on the
  // mouse X coordinate.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_TOP);
  PlaceCursor(kWindowAwayFromEdge, kMenuBarSize + kCursorNearShelfY);
  EXPECT_EQ(gfx::Point(kWindowAwayFromEdge,
                       kMenuBarSize + kShelfSize + kWindowHeight / 2),
            DoFindAnchorPoint());

  // Left shelf. Expect the app list to be at the left centered on the
  // mouse Y coordinate.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_LEFT);
  PlaceCursor(kCursorNearShelfX, kWindowAwayFromEdge);
  EXPECT_EQ(gfx::Point(kShelfSize + kWindowWidth / 2, kWindowAwayFromEdge),
            DoFindAnchorPoint());

  // Right shelf. Expect the app list to be at the right centered on the
  // mouse Y coordinate.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_RIGHT);
  PlaceCursor(kScreenWidth - kCursorNearShelfX, kWindowAwayFromEdge);
  EXPECT_EQ(gfx::Point(kScreenWidth - kShelfSize - kWindowWidth / 2,
                       kWindowAwayFromEdge),
            DoFindAnchorPoint());

  // Bottom shelf. Mouse near left edge. App list must not go off screen.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_BOTTOM);
  PlaceCursor(kWindowNearEdge, kScreenHeight - kCursorOnShelf);
  EXPECT_EQ(gfx::Point(kWindowWidth / 2,
                       kScreenHeight - kShelfSize - kWindowHeight / 2),
            DoFindAnchorPoint());

  // Bottom shelf. Mouse near right edge. App list must not go off screen.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_BOTTOM);
  PlaceCursor(kScreenWidth - kWindowNearEdge, kScreenHeight - kCursorOnShelf);
  EXPECT_EQ(gfx::Point(kScreenWidth - kWindowWidth / 2,
                       kScreenHeight - kShelfSize - kWindowHeight / 2),
            DoFindAnchorPoint());
}
