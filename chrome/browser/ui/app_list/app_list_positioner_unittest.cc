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
  virtual void SetUp() OVERRIDE {
    display_.set_bounds(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
    display_.set_work_area(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
    gfx::Size view_size(kWindowWidth, kWindowHeight);
    positioner_.reset(
        new AppListPositioner(display_, view_size, kMinDistanceFromEdge));
    cursor_ = gfx::Point();
    shelf_rect_ = gfx::Rect();
  }

  void SetShelfRect(int x, int y, int width, int height) {
    shelf_rect_ = gfx::Rect(x, y, width, height);
  }

  // Sets up the test environment with the shelf along a given edge of the work
  // area.
  void PlaceShelf(AppListPositioner::ScreenEdge edge) {
    const gfx::Rect& work_area = display_.work_area();
    switch (edge) {
      case AppListPositioner::SCREEN_EDGE_UNKNOWN:
        shelf_rect_ = gfx::Rect();
        break;
      case AppListPositioner::SCREEN_EDGE_LEFT:
        shelf_rect_ = gfx::Rect(
            work_area.x(), work_area.y(), kShelfSize, work_area.height());
        break;
      case AppListPositioner::SCREEN_EDGE_RIGHT:
        shelf_rect_ = gfx::Rect(work_area.x() + work_area.width() - kShelfSize,
                                work_area.y(),
                                kShelfSize,
                                work_area.height());
        break;
      case AppListPositioner::SCREEN_EDGE_TOP:
        shelf_rect_ = gfx::Rect(
            work_area.x(), work_area.y(), work_area.width(), kShelfSize);
        break;
      case AppListPositioner::SCREEN_EDGE_BOTTOM:
        shelf_rect_ = gfx::Rect(work_area.x(),
                                work_area.y() + work_area.height() - kShelfSize,
                                work_area.width(),
                                kShelfSize);
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
    return positioner_->GetAnchorPointForShelfCorner(shelf_edge, shelf_rect_);
  }

  gfx::Point DoGetAnchorPointForShelfCursor(
      AppListPositioner::ScreenEdge shelf_edge) const {
    return positioner_->GetAnchorPointForShelfCursor(
        shelf_edge, shelf_rect_, cursor_);
  }

  AppListPositioner::ScreenEdge DoGetShelfEdge() const {
    return positioner_->GetShelfEdge(shelf_rect_);
  }

  int DoGetCursorDistanceFromShelf(AppListPositioner::ScreenEdge shelf_edge)
      const {
    return positioner_->GetCursorDistanceFromShelf(
        shelf_edge, shelf_rect_, cursor_);
  }

 private:
  gfx::Display display_;
  scoped_ptr<AppListPositioner> positioner_;
  gfx::Point cursor_;
  gfx::Rect shelf_rect_;
};

TEST_F(AppListPositionerUnitTest, ScreenCorner) {
  // Position the app list in a corner of the screen.
  // Top-left corner.
  EXPECT_EQ(gfx::Point(kWindowWidth / 2 + kMinDistanceFromEdge,
                       kWindowHeight / 2 + kMinDistanceFromEdge),
            DoGetAnchorPointForScreenCorner(
                AppListPositioner::SCREEN_CORNER_TOP_LEFT));

  // Top-right corner.
  EXPECT_EQ(gfx::Point(kScreenWidth - kWindowWidth / 2 - kMinDistanceFromEdge,
                       kWindowHeight / 2 + kMinDistanceFromEdge),
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
  SetShelfRect(0, 0, kShelfSize, kScreenHeight);
  EXPECT_EQ(
      gfx::Point(kShelfSize + kWindowWidth / 2 + kMinDistanceFromEdge,
                 kWindowHeight / 2 + kMinDistanceFromEdge),
      DoGetAnchorPointForShelfCorner(AppListPositioner::SCREEN_EDGE_LEFT));

  // Shelf on right. Expect app list in top-right corner.
  SetShelfRect(kScreenWidth - kShelfSize, 0, kShelfSize, kScreenHeight);
  EXPECT_EQ(
      gfx::Point(
          kScreenWidth - kShelfSize - kWindowWidth / 2 - kMinDistanceFromEdge,
          kWindowHeight / 2 + kMinDistanceFromEdge),
      DoGetAnchorPointForShelfCorner(AppListPositioner::SCREEN_EDGE_RIGHT));

  // Shelf on top. Expect app list in top-left corner.
  SetShelfRect(0, 0, kScreenWidth, kShelfSize);
  EXPECT_EQ(gfx::Point(kWindowWidth / 2 + kMinDistanceFromEdge,
                       kShelfSize + kWindowHeight / 2 + kMinDistanceFromEdge),
            DoGetAnchorPointForShelfCorner(AppListPositioner::SCREEN_EDGE_TOP));

  // Shelf on bottom. Expect app list in bottom-left corner.
  SetShelfRect(0, kScreenHeight - kShelfSize, kScreenWidth, kShelfSize);
  EXPECT_EQ(
      gfx::Point(kWindowWidth / 2 + kMinDistanceFromEdge,
                 kScreenHeight - kShelfSize - kWindowHeight / 2 -
                     kMinDistanceFromEdge),
      DoGetAnchorPointForShelfCorner(AppListPositioner::SCREEN_EDGE_BOTTOM));
}

TEST_F(AppListPositionerUnitTest, ShelfCursor) {
  // Position the app list on the shelf, aligned with the mouse cursor.

  // Shelf on left. Expect app list in top-left corner.
  SetShelfRect(0, 0, kShelfSize, kScreenHeight);
  PlaceCursor(kCursorIgnore, kWindowAwayFromEdge);
  EXPECT_EQ(
      gfx::Point(kShelfSize + kWindowWidth / 2 + kMinDistanceFromEdge,
                 kWindowAwayFromEdge),
      DoGetAnchorPointForShelfCursor(AppListPositioner::SCREEN_EDGE_LEFT));

  // Shelf on right. Expect app list in top-right corner.
  SetShelfRect(kScreenWidth - kShelfSize, 0, kShelfSize, kScreenHeight);
  PlaceCursor(kCursorIgnore, kWindowAwayFromEdge);
  EXPECT_EQ(
      gfx::Point(
          kScreenWidth - kShelfSize - kWindowWidth / 2 - kMinDistanceFromEdge,
          kWindowAwayFromEdge),
      DoGetAnchorPointForShelfCursor(AppListPositioner::SCREEN_EDGE_RIGHT));

  // Shelf on top. Expect app list in top-left corner.
  SetShelfRect(0, 0, kScreenWidth, kShelfSize);
  PlaceCursor(kWindowAwayFromEdge, kCursorIgnore);
  EXPECT_EQ(gfx::Point(kWindowAwayFromEdge,
                       kShelfSize + kWindowHeight / 2 + kMinDistanceFromEdge),
            DoGetAnchorPointForShelfCursor(AppListPositioner::SCREEN_EDGE_TOP));

  // Shelf on bottom. Expect app list in bottom-left corner.
  SetShelfRect(0, kScreenHeight - kShelfSize, kScreenWidth, kShelfSize);
  PlaceCursor(kWindowAwayFromEdge, kCursorIgnore);
  EXPECT_EQ(
      gfx::Point(kWindowAwayFromEdge,
                 kScreenHeight - kShelfSize - kWindowHeight / 2 -
                     kMinDistanceFromEdge),
      DoGetAnchorPointForShelfCursor(AppListPositioner::SCREEN_EDGE_BOTTOM));

  // Shelf on bottom. Mouse near left edge. App list must not go off screen.
  SetShelfRect(0, kScreenHeight - kShelfSize, kScreenWidth, kShelfSize);
  PlaceCursor(kWindowNearEdge, kCursorIgnore);
  EXPECT_EQ(
      gfx::Point(kWindowWidth / 2 + kMinDistanceFromEdge,
                 kScreenHeight - kShelfSize - kWindowHeight / 2 -
                     kMinDistanceFromEdge),
      DoGetAnchorPointForShelfCursor(AppListPositioner::SCREEN_EDGE_BOTTOM));

  // Shelf on bottom. Mouse near right edge. App list must not go off screen.
  SetShelfRect(0, kScreenHeight - kShelfSize, kScreenWidth, kShelfSize);
  PlaceCursor(kScreenWidth - kWindowNearEdge, kCursorIgnore);
  EXPECT_EQ(
      gfx::Point(kScreenWidth - kWindowWidth / 2 - kMinDistanceFromEdge,
                 kScreenHeight - kShelfSize - kWindowHeight / 2 -
                     kMinDistanceFromEdge),
      DoGetAnchorPointForShelfCursor(AppListPositioner::SCREEN_EDGE_BOTTOM));
}

TEST_F(AppListPositionerUnitTest, GetShelfEdge) {
  // Shelf on left.
  SetShelfRect(0, 0, kShelfSize, kScreenHeight);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_LEFT, DoGetShelfEdge());

  // Shelf on right.
  SetShelfRect(kScreenWidth - kShelfSize, 0, kShelfSize, kScreenHeight);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_RIGHT, DoGetShelfEdge());

  // Shelf on top.
  SetShelfRect(0, 0, kScreenWidth, kShelfSize);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_TOP, DoGetShelfEdge());

  // Shelf on bottom.
  SetShelfRect(0, kScreenHeight - kShelfSize, kScreenWidth, kShelfSize);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_BOTTOM, DoGetShelfEdge());

  // A couple of inconclusive cases, which should return unknown.
  SetShelfRect(0, 0, 0, 0);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_UNKNOWN, DoGetShelfEdge());
  SetShelfRect(-10, 0, kScreenWidth, kShelfSize);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_UNKNOWN, DoGetShelfEdge());
  SetShelfRect(10, 0, kScreenWidth - 20, kShelfSize);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_UNKNOWN, DoGetShelfEdge());
  SetShelfRect(0, kShelfSize, kScreenWidth, 60);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_UNKNOWN, DoGetShelfEdge());
}

TEST_F(AppListPositionerUnitTest, GetCursorDistanceFromShelf) {
  // Shelf on left.
  SetShelfRect(0, 0, kShelfSize, kScreenHeight);
  PlaceCursor(kWindowAwayFromEdge, kCursorIgnore);
  EXPECT_EQ(kWindowAwayFromEdge - kShelfSize,
            DoGetCursorDistanceFromShelf(AppListPositioner::SCREEN_EDGE_LEFT));

  // Shelf on right.
  SetShelfRect(kScreenWidth - kShelfSize, 0, kShelfSize, kScreenHeight);
  PlaceCursor(kScreenWidth - kWindowAwayFromEdge, kCursorIgnore);
  EXPECT_EQ(kWindowAwayFromEdge - kShelfSize,
            DoGetCursorDistanceFromShelf(AppListPositioner::SCREEN_EDGE_RIGHT));

  // Shelf on top.
  SetShelfRect(0, 0, kScreenWidth, kShelfSize);
  PlaceCursor(kCursorIgnore, kWindowAwayFromEdge);
  EXPECT_EQ(kWindowAwayFromEdge - kShelfSize,
            DoGetCursorDistanceFromShelf(AppListPositioner::SCREEN_EDGE_TOP));

  // Shelf on bottom.
  SetShelfRect(0, kScreenHeight - kShelfSize, kScreenWidth, kShelfSize);
  PlaceCursor(kCursorIgnore, kScreenHeight - kWindowAwayFromEdge);
  EXPECT_EQ(
      kWindowAwayFromEdge - kShelfSize,
      DoGetCursorDistanceFromShelf(AppListPositioner::SCREEN_EDGE_BOTTOM));

  // Shelf on bottom. Cursor inside shelf; expect 0.
  SetShelfRect(0, kScreenHeight - kShelfSize, kScreenWidth, kShelfSize);
  PlaceCursor(kCursorIgnore, kScreenHeight - kCursorOnShelf);
  EXPECT_EQ(
      0, DoGetCursorDistanceFromShelf(AppListPositioner::SCREEN_EDGE_BOTTOM));
}
