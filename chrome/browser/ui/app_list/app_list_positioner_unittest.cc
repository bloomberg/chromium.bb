// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_positioner.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kScreenWidth = 800;
const int kScreenHeight = 600;

const int kWindowWidth = 100;
const int kWindowHeight = 200;

// Size of the menu bar along the top of the screen.
const int kMenuBarSize = 22;
// Size of the normal (non-hidden) shelf.
const int kShelfSize = 30;

// The distance the shelf will appear from the edge of the screen.
const int kMinDistanceFromEdge = 3;

// A cursor position that is within the shelf. This must be < kShelfSize.
const int kCursorOnShelf = kShelfSize / 2;
// A cursor position that should be ignored.
const int kCursorIgnore = -300;

// A position for the center of the window that causes the window to overlap the
// edge of the screen. This must be < kWindowWidth / 2 and < kWindowHeight / 2.
const int kWindowNearEdge = kWindowWidth / 4;
// A position for the center of the window that places the window away from all
// edges of the screen. This must be > kWindowWidth / 2, > kWindowHeight / 2, <
// kScreenWidth - kWindowWidth / 2 and < kScreenHeight - kWindowHeight / 2.
const int kWindowAwayFromEdge = 158;

}  // namespace

class AppListPositionerUnitTest : public testing::Test {
 public:
  void ResetPositioner() {
    gfx::Size view_size(kWindowWidth, kWindowHeight);
    positioner_.reset(
        new AppListPositioner(display_, view_size, kMinDistanceFromEdge));
  }

  virtual void SetUp() OVERRIDE {
    display_.set_bounds(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
    // Assume there is a menu bar at the top of the screen, as on Mac and Unity.
    // This is for cases where the work area does not fill the entire screen.
    display_.set_work_area(
        gfx::Rect(0, kMenuBarSize, kScreenWidth, kScreenHeight - kMenuBarSize));
    ResetPositioner();
    cursor_ = gfx::Point();
  }

  // Sets up the test environment with the shelf along a given edge of the work
  // area.
  void PlaceShelf(AppListPositioner::ScreenEdge edge) {
    ResetPositioner();
    switch (edge) {
      case AppListPositioner::SCREEN_EDGE_UNKNOWN:
        break;
      case AppListPositioner::SCREEN_EDGE_LEFT:
        positioner_->WorkAreaInset(kShelfSize, 0, 0, 0);
        break;
      case AppListPositioner::SCREEN_EDGE_RIGHT:
        positioner_->WorkAreaInset(0, 0, kShelfSize, 0);
        break;
      case AppListPositioner::SCREEN_EDGE_TOP:
        positioner_->WorkAreaInset(0, kShelfSize, 0, 0);
        break;
      case AppListPositioner::SCREEN_EDGE_BOTTOM:
        positioner_->WorkAreaInset(0, 0, 0, kShelfSize);
        break;
    }
  }

  // Set up the test mouse cursor in a given location.
  void PlaceCursor(int x, int y) {
    cursor_ = gfx::Point(x, y);
  }

  gfx::Point DoGetAnchorPointForScreenCorner(
      AppListPositioner::ScreenCorner corner) const {
    return positioner_->GetAnchorPointForScreenCorner(corner);
  }

  gfx::Point DoGetAnchorPointForShelfCorner(
      AppListPositioner::ScreenEdge shelf_edge) const {
    return positioner_->GetAnchorPointForShelfCorner(shelf_edge);
  }

  gfx::Point DoGetAnchorPointForShelfCenter(
      AppListPositioner::ScreenEdge shelf_edge) const {
    return positioner_->GetAnchorPointForShelfCenter(shelf_edge);
  }

  gfx::Point DoGetAnchorPointForShelfCursor(
      AppListPositioner::ScreenEdge shelf_edge) const {
    return positioner_->GetAnchorPointForShelfCursor(shelf_edge, cursor_);
  }

  AppListPositioner::ScreenEdge DoGetShelfEdge(
      const gfx::Rect& shelf_rect) const {
    return positioner_->GetShelfEdge(shelf_rect);
  }

  int DoGetCursorDistanceFromShelf(
      AppListPositioner::ScreenEdge shelf_edge) const {
    return positioner_->GetCursorDistanceFromShelf(shelf_edge, cursor_);
  }

 private:
  gfx::Display display_;
  scoped_ptr<AppListPositioner> positioner_;
  gfx::Point cursor_;
};

TEST_F(AppListPositionerUnitTest, ScreenCorner) {
  // Position the app list in a corner of the screen.
  // Top-left corner.
  EXPECT_EQ(gfx::Point(kWindowWidth / 2 + kMinDistanceFromEdge,
                       kMenuBarSize + kWindowHeight / 2 + kMinDistanceFromEdge),
            DoGetAnchorPointForScreenCorner(
                AppListPositioner::SCREEN_CORNER_TOP_LEFT));

  // Top-right corner.
  EXPECT_EQ(gfx::Point(kScreenWidth - kWindowWidth / 2 - kMinDistanceFromEdge,
                       kMenuBarSize + kWindowHeight / 2 + kMinDistanceFromEdge),
            DoGetAnchorPointForScreenCorner(
                AppListPositioner::SCREEN_CORNER_TOP_RIGHT));

  // Bottom-left corner.
  EXPECT_EQ(
      gfx::Point(kWindowWidth / 2 + kMinDistanceFromEdge,
                 kScreenHeight - kWindowHeight / 2 - kMinDistanceFromEdge),
      DoGetAnchorPointForScreenCorner(
          AppListPositioner::SCREEN_CORNER_BOTTOM_LEFT));

  // Bottom-right corner.
  EXPECT_EQ(
      gfx::Point(kScreenWidth - kWindowWidth / 2 - kMinDistanceFromEdge,
                 kScreenHeight - kWindowHeight / 2 - kMinDistanceFromEdge),
      DoGetAnchorPointForScreenCorner(
          AppListPositioner::SCREEN_CORNER_BOTTOM_RIGHT));
}

TEST_F(AppListPositionerUnitTest, ShelfCorner) {
  // Position the app list on the shelf, aligned with the top or left corner.
  // Shelf on left. Expect app list in top-left corner.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_LEFT);
  EXPECT_EQ(
      gfx::Point(kShelfSize + kWindowWidth / 2 + kMinDistanceFromEdge,
                 kMenuBarSize + kWindowHeight / 2 + kMinDistanceFromEdge),
      DoGetAnchorPointForShelfCorner(AppListPositioner::SCREEN_EDGE_LEFT));

  // Shelf on right. Expect app list in top-right corner.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_RIGHT);
  EXPECT_EQ(
      gfx::Point(
          kScreenWidth - kShelfSize - kWindowWidth / 2 - kMinDistanceFromEdge,
          kMenuBarSize + kWindowHeight / 2 + kMinDistanceFromEdge),
      DoGetAnchorPointForShelfCorner(AppListPositioner::SCREEN_EDGE_RIGHT));

  // Shelf on top. Expect app list in top-left corner.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_TOP);
  EXPECT_EQ(gfx::Point(kWindowWidth / 2 + kMinDistanceFromEdge,
                       kMenuBarSize + kShelfSize + kWindowHeight / 2 +
                           kMinDistanceFromEdge),
            DoGetAnchorPointForShelfCorner(AppListPositioner::SCREEN_EDGE_TOP));

  // Shelf on bottom. Expect app list in bottom-left corner.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_BOTTOM);
  EXPECT_EQ(
      gfx::Point(kWindowWidth / 2 + kMinDistanceFromEdge,
                 kScreenHeight - kShelfSize - kWindowHeight / 2 -
                     kMinDistanceFromEdge),
      DoGetAnchorPointForShelfCorner(AppListPositioner::SCREEN_EDGE_BOTTOM));
}

TEST_F(AppListPositionerUnitTest, ShelfCenter) {
  // Position the app list on the shelf, aligned with the shelf center.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_LEFT);
  // Shelf on left. Expect app list to be center-left.
  EXPECT_EQ(
      gfx::Point(kShelfSize + kWindowWidth / 2 + kMinDistanceFromEdge,
                 (kMenuBarSize + kScreenHeight) / 2),
      DoGetAnchorPointForShelfCenter(AppListPositioner::SCREEN_EDGE_LEFT));

  // Shelf on right. Expect app list to be center-right.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_RIGHT);
  EXPECT_EQ(
      gfx::Point(
          kScreenWidth - kShelfSize - kWindowWidth / 2 - kMinDistanceFromEdge,
          (kMenuBarSize + kScreenHeight) / 2),
      DoGetAnchorPointForShelfCenter(AppListPositioner::SCREEN_EDGE_RIGHT));

  // Shelf on top. Expect app list to be top-center.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_TOP);
  EXPECT_EQ(gfx::Point(kScreenWidth / 2,
                       kMenuBarSize + kShelfSize + kWindowHeight / 2 +
                           kMinDistanceFromEdge),
            DoGetAnchorPointForShelfCenter(AppListPositioner::SCREEN_EDGE_TOP));

  // Shelf on bottom. Expect app list to be bottom-center.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_BOTTOM);
  EXPECT_EQ(
      gfx::Point(kScreenWidth / 2,
                 kScreenHeight - kShelfSize - kWindowHeight / 2 -
                     kMinDistanceFromEdge),
      DoGetAnchorPointForShelfCenter(AppListPositioner::SCREEN_EDGE_BOTTOM));
}

TEST_F(AppListPositionerUnitTest, ShelfCursor) {
  // Position the app list on the shelf, aligned with the mouse cursor.

  // Shelf on left. Expect app list in top-left corner.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_LEFT);
  PlaceCursor(kCursorIgnore, kWindowAwayFromEdge);
  EXPECT_EQ(
      gfx::Point(kShelfSize + kWindowWidth / 2 + kMinDistanceFromEdge,
                 kWindowAwayFromEdge),
      DoGetAnchorPointForShelfCursor(AppListPositioner::SCREEN_EDGE_LEFT));

  // Shelf on right. Expect app list in top-right corner.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_RIGHT);
  PlaceCursor(kCursorIgnore, kWindowAwayFromEdge);
  EXPECT_EQ(
      gfx::Point(
          kScreenWidth - kShelfSize - kWindowWidth / 2 - kMinDistanceFromEdge,
          kWindowAwayFromEdge),
      DoGetAnchorPointForShelfCursor(AppListPositioner::SCREEN_EDGE_RIGHT));

  // Shelf on top. Expect app list in top-left corner.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_TOP);
  PlaceCursor(kWindowAwayFromEdge, kCursorIgnore);
  EXPECT_EQ(gfx::Point(kWindowAwayFromEdge,
                       kMenuBarSize + kShelfSize + kWindowHeight / 2 +
                           kMinDistanceFromEdge),
            DoGetAnchorPointForShelfCursor(AppListPositioner::SCREEN_EDGE_TOP));

  // Shelf on bottom. Expect app list in bottom-left corner.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_BOTTOM);
  PlaceCursor(kWindowAwayFromEdge, kCursorIgnore);
  EXPECT_EQ(
      gfx::Point(kWindowAwayFromEdge,
                 kScreenHeight - kShelfSize - kWindowHeight / 2 -
                     kMinDistanceFromEdge),
      DoGetAnchorPointForShelfCursor(AppListPositioner::SCREEN_EDGE_BOTTOM));

  // Shelf on bottom. Mouse near left edge. App list must not go off screen.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_BOTTOM);
  PlaceCursor(kWindowNearEdge, kCursorIgnore);
  EXPECT_EQ(
      gfx::Point(kWindowWidth / 2 + kMinDistanceFromEdge,
                 kScreenHeight - kShelfSize - kWindowHeight / 2 -
                     kMinDistanceFromEdge),
      DoGetAnchorPointForShelfCursor(AppListPositioner::SCREEN_EDGE_BOTTOM));

  // Shelf on bottom. Mouse near right edge. App list must not go off screen.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_BOTTOM);
  PlaceCursor(kScreenWidth - kWindowNearEdge, kCursorIgnore);
  EXPECT_EQ(
      gfx::Point(kScreenWidth - kWindowWidth / 2 - kMinDistanceFromEdge,
                 kScreenHeight - kShelfSize - kWindowHeight / 2 -
                     kMinDistanceFromEdge),
      DoGetAnchorPointForShelfCursor(AppListPositioner::SCREEN_EDGE_BOTTOM));
}

TEST_F(AppListPositionerUnitTest, GetShelfEdge) {
  gfx::Rect shelf_rect;
  // Shelf on left.
  shelf_rect =
      gfx::Rect(0, kMenuBarSize, kShelfSize, kScreenHeight - kMenuBarSize);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_LEFT, DoGetShelfEdge(shelf_rect));

  // Shelf on right.
  shelf_rect = gfx::Rect(kScreenWidth - kShelfSize,
                         kMenuBarSize,
                         kShelfSize,
                         kScreenHeight - kMenuBarSize);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_RIGHT, DoGetShelfEdge(shelf_rect));

  // Shelf on top.
  shelf_rect = gfx::Rect(0, 0, kScreenWidth, kShelfSize);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_TOP, DoGetShelfEdge(shelf_rect));

  // Shelf on bottom.
  shelf_rect =
      gfx::Rect(0, kScreenHeight - kShelfSize, kScreenWidth, kShelfSize);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_BOTTOM, DoGetShelfEdge(shelf_rect));

  // A couple of inconclusive cases, which should return unknown.
  shelf_rect = gfx::Rect();
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_UNKNOWN, DoGetShelfEdge(shelf_rect));
  shelf_rect = gfx::Rect(-10, 0, kScreenWidth, kShelfSize);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_UNKNOWN, DoGetShelfEdge(shelf_rect));
  shelf_rect = gfx::Rect(10, 0, kScreenWidth - 20, kShelfSize);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_UNKNOWN, DoGetShelfEdge(shelf_rect));
  shelf_rect = gfx::Rect(0, kShelfSize, kScreenWidth, 60);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_UNKNOWN, DoGetShelfEdge(shelf_rect));
}

TEST_F(AppListPositionerUnitTest, GetCursorDistanceFromShelf) {
  // Shelf on left.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_LEFT);
  PlaceCursor(kWindowAwayFromEdge, kCursorIgnore);
  EXPECT_EQ(kWindowAwayFromEdge - kShelfSize,
            DoGetCursorDistanceFromShelf(AppListPositioner::SCREEN_EDGE_LEFT));

  // Shelf on right.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_RIGHT);
  PlaceCursor(kScreenWidth - kWindowAwayFromEdge, kCursorIgnore);
  EXPECT_EQ(kWindowAwayFromEdge - kShelfSize,
            DoGetCursorDistanceFromShelf(AppListPositioner::SCREEN_EDGE_RIGHT));

  // Shelf on top.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_TOP);
  PlaceCursor(kCursorIgnore, kMenuBarSize + kWindowAwayFromEdge);
  EXPECT_EQ(kWindowAwayFromEdge - kShelfSize,
            DoGetCursorDistanceFromShelf(AppListPositioner::SCREEN_EDGE_TOP));

  // Shelf on bottom.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_BOTTOM);
  PlaceCursor(kCursorIgnore, kScreenHeight - kWindowAwayFromEdge);
  EXPECT_EQ(
      kWindowAwayFromEdge - kShelfSize,
      DoGetCursorDistanceFromShelf(AppListPositioner::SCREEN_EDGE_BOTTOM));

  // Shelf on bottom. Cursor inside shelf; expect 0.
  PlaceShelf(AppListPositioner::SCREEN_EDGE_BOTTOM);
  PlaceCursor(kCursorIgnore, kScreenHeight - kCursorOnShelf);
  EXPECT_EQ(
      0, DoGetCursorDistanceFromShelf(AppListPositioner::SCREEN_EDGE_BOTTOM));
}
