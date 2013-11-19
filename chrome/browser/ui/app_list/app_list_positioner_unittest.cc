// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/app_list_positioner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kScreenWidth = 800;
const int kScreenHeight = 600;
}  // namespace

class AppListPositionerUnitTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    display_.set_bounds(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
    display_.set_work_area(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
    gfx::Size view_size(100, 200);
    positioner_.reset(new AppListPositioner(display_, view_size, 3));
  }

  const AppListPositioner& positioner() const { return *positioner_; }

 private:
  gfx::Display display_;
  scoped_ptr<AppListPositioner> positioner_;
};

TEST_F(AppListPositionerUnitTest, ScreenCorner) {
  // Position the app list in a corner of the screen.
  // Top-left corner.
  EXPECT_EQ(gfx::Point(53, 103),
            positioner().GetAnchorPointForScreenCorner(
                AppListPositioner::SCREEN_CORNER_TOP_LEFT));

  // Top-right corner.
  EXPECT_EQ(gfx::Point(kScreenWidth - 53, 103),
            positioner().GetAnchorPointForScreenCorner(
                AppListPositioner::SCREEN_CORNER_TOP_RIGHT));

  // Bottom-left corner.
  EXPECT_EQ(gfx::Point(53, kScreenHeight - 103),
            positioner().GetAnchorPointForScreenCorner(
                AppListPositioner::SCREEN_CORNER_BOTTOM_LEFT));

  // Bottom-right corner.
  EXPECT_EQ(gfx::Point(kScreenWidth - 53, kScreenHeight - 103),
            positioner().GetAnchorPointForScreenCorner(
                AppListPositioner::SCREEN_CORNER_BOTTOM_RIGHT));
}

TEST_F(AppListPositionerUnitTest, ShelfCorner) {
  // Position the app list on the shelf, aligned with the top or left corner.
  gfx::Rect shelf_rect;
  // Shelf on left. Expect app list in top-left corner.
  shelf_rect = gfx::Rect(0, 0, 30, kScreenHeight);
  EXPECT_EQ(gfx::Point(83, 103),
            positioner().GetAnchorPointForShelfCorner(
                AppListPositioner::SCREEN_EDGE_LEFT, shelf_rect));

  // Shelf on right. Expect app list in top-right corner.
  shelf_rect = gfx::Rect(kScreenWidth - 30, 0, 30, kScreenHeight);
  EXPECT_EQ(gfx::Point(kScreenWidth - 83, 103),
            positioner().GetAnchorPointForShelfCorner(
                AppListPositioner::SCREEN_EDGE_RIGHT, shelf_rect));

  // Shelf on top. Expect app list in top-left corner.
  shelf_rect = gfx::Rect(0, 0, kScreenWidth, 30);
  EXPECT_EQ(gfx::Point(53, 133),
            positioner().GetAnchorPointForShelfCorner(
                AppListPositioner::SCREEN_EDGE_TOP, shelf_rect));

  // Shelf on bottom. Expect app list in bottom-left corner.
  shelf_rect = gfx::Rect(0, kScreenHeight - 30, kScreenWidth, 30);
  EXPECT_EQ(gfx::Point(53, kScreenHeight - 133),
            positioner().GetAnchorPointForShelfCorner(
                AppListPositioner::SCREEN_EDGE_BOTTOM, shelf_rect));
}

TEST_F(AppListPositionerUnitTest, ShelfCursor) {
  // Position the app list on the shelf, aligned with the mouse cursor.
  gfx::Rect shelf_rect;
  gfx::Point cursor;
  // Shelf on left. Expect app list in top-left corner.
  shelf_rect = gfx::Rect(0, 0, 30, kScreenHeight);
  cursor = gfx::Point(-300, 158);  // X coordinate should be ignored.
  EXPECT_EQ(gfx::Point(83, 158),
            positioner().GetAnchorPointForShelfCursor(
                AppListPositioner::SCREEN_EDGE_LEFT, shelf_rect, cursor));

  // Shelf on right. Expect app list in top-right corner.
  shelf_rect = gfx::Rect(kScreenWidth - 30, 0, 30, kScreenHeight);
  cursor = gfx::Point(-300, 158);  // X coordinate should be ignored.
  EXPECT_EQ(gfx::Point(kScreenWidth - 83, 158),
            positioner().GetAnchorPointForShelfCursor(
                AppListPositioner::SCREEN_EDGE_RIGHT, shelf_rect, cursor));

  // Shelf on top. Expect app list in top-left corner.
  shelf_rect = gfx::Rect(0, 0, kScreenWidth, 30);
  cursor = gfx::Point(158, -300);  // Y coordinate should be ignored.
  EXPECT_EQ(gfx::Point(158, 133),
            positioner().GetAnchorPointForShelfCursor(
                AppListPositioner::SCREEN_EDGE_TOP, shelf_rect, cursor));

  // Shelf on bottom. Expect app list in bottom-left corner.
  shelf_rect = gfx::Rect(0, kScreenHeight - 30, kScreenWidth, 30);
  cursor = gfx::Point(158, -300);  // Y coordinate should be ignored.
  EXPECT_EQ(gfx::Point(158, kScreenHeight - 133),
            positioner().GetAnchorPointForShelfCursor(
                AppListPositioner::SCREEN_EDGE_BOTTOM, shelf_rect, cursor));

  // Shelf on bottom. Mouse near left edge. App list must not go off screen.
  shelf_rect = gfx::Rect(0, kScreenHeight - 30, kScreenWidth, 30);
  cursor = gfx::Point(20, -300);
  EXPECT_EQ(gfx::Point(53, kScreenHeight - 133),
            positioner().GetAnchorPointForShelfCursor(
                AppListPositioner::SCREEN_EDGE_BOTTOM, shelf_rect, cursor));

  // Shelf on bottom. Mouse near right edge. App list must not go off screen.
  shelf_rect = gfx::Rect(0, kScreenHeight - 30, kScreenWidth, 30);
  cursor = gfx::Point(kScreenWidth - 20, -300);
  EXPECT_EQ(gfx::Point(kScreenWidth - 53, kScreenHeight - 133),
            positioner().GetAnchorPointForShelfCursor(
                AppListPositioner::SCREEN_EDGE_BOTTOM, shelf_rect, cursor));
}

TEST_F(AppListPositionerUnitTest, GetShelfEdge) {
  gfx::Rect shelf_rect;

  // Shelf on left.
  shelf_rect = gfx::Rect(0, 0, 30, kScreenHeight);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_LEFT,
            positioner().GetShelfEdge(shelf_rect));

  // Shelf on right.
  shelf_rect = gfx::Rect(kScreenWidth - 30, 0, 30, kScreenHeight);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_RIGHT,
            positioner().GetShelfEdge(shelf_rect));

  // Shelf on top.
  shelf_rect = gfx::Rect(0, 0, kScreenWidth, 30);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_TOP,
            positioner().GetShelfEdge(shelf_rect));

  // Shelf on bottom.
  shelf_rect = gfx::Rect(0, kScreenHeight - 30, kScreenWidth, 30);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_BOTTOM,
            positioner().GetShelfEdge(shelf_rect));

  // A couple of inconclusive cases, which should return unknown.
  shelf_rect = gfx::Rect();
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_UNKNOWN,
            positioner().GetShelfEdge(shelf_rect));
  shelf_rect = gfx::Rect(-10, 0, kScreenWidth, 30);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_UNKNOWN,
            positioner().GetShelfEdge(shelf_rect));
  shelf_rect = gfx::Rect(10, 0, kScreenWidth - 20, 30);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_UNKNOWN,
            positioner().GetShelfEdge(shelf_rect));
  shelf_rect = gfx::Rect(0, 30, kScreenWidth, 60);
  EXPECT_EQ(AppListPositioner::SCREEN_EDGE_UNKNOWN,
            positioner().GetShelfEdge(shelf_rect));
}

TEST_F(AppListPositionerUnitTest, GetCursorDistanceFromShelf) {
  gfx::Rect shelf_rect;
  gfx::Point cursor;

  // Shelf on left.
  shelf_rect = gfx::Rect(0, 0, 30, kScreenHeight);
  cursor = gfx::Point(158, -300);  // Y coordinate should be ignored.
  EXPECT_EQ(128,
            positioner().GetCursorDistanceFromShelf(
                AppListPositioner::SCREEN_EDGE_LEFT, shelf_rect, cursor));

  // Shelf on right.
  shelf_rect = gfx::Rect(kScreenWidth - 30, 0, 30, kScreenHeight);
  cursor = gfx::Point(kScreenWidth - 158, -300);
  EXPECT_EQ(128,
            positioner().GetCursorDistanceFromShelf(
                AppListPositioner::SCREEN_EDGE_RIGHT, shelf_rect, cursor));

  // Shelf on top.
  shelf_rect = gfx::Rect(0, 0, kScreenWidth, 30);
  cursor = gfx::Point(-300, 158);  // X coordinate should be ignored.
  EXPECT_EQ(128,
            positioner().GetCursorDistanceFromShelf(
                AppListPositioner::SCREEN_EDGE_TOP, shelf_rect, cursor));

  // Shelf on bottom.
  shelf_rect = gfx::Rect(0, kScreenHeight - 30, kScreenWidth, 30);
  cursor = gfx::Point(-300, kScreenHeight - 158);
  EXPECT_EQ(128,
            positioner().GetCursorDistanceFromShelf(
                AppListPositioner::SCREEN_EDGE_BOTTOM, shelf_rect, cursor));

  // Shelf on bottom. Cursor inside shelf; expect 0.
  shelf_rect = gfx::Rect(0, kScreenHeight - 30, kScreenWidth, 30);
  cursor = gfx::Point(-300, kScreenHeight - 20);
  EXPECT_EQ(0,
            positioner().GetCursorDistanceFromShelf(
                AppListPositioner::SCREEN_EDGE_BOTTOM, shelf_rect, cursor));
}
