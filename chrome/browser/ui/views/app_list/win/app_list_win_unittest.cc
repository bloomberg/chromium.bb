// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/win/app_list_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace {
const int kScreenWidth = 800;
const int kScreenHeight = 600;
}  // namespace

class AppListWinUnitTest : public testing::Test {
};

TEST_F(AppListWinUnitTest, FindAnchorPointNoTaskbar) {
  // Position the app list when there is no taskbar on the display.
  gfx::Display display;
  display.set_bounds(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
  display.set_work_area(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
  gfx::Size view_size(100, 200);
  gfx::Point cursor(1000, -100);  // Should be ignored.
  // Expect the app list to be in the bottom-left corner.
  EXPECT_EQ(gfx::Point(53, kScreenHeight - 103),
            FindAnchorPoint(view_size, display, cursor, gfx::Rect()));
}

TEST_F(AppListWinUnitTest, FindAnchorPointMouseOffTaskbar) {
  // Position the app list when the mouse is away from the taskbar.
  gfx::Display display;
  display.set_bounds(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
  display.set_work_area(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
  gfx::Size view_size(100, 200);
  gfx::Rect taskbar_rect;
  gfx::Point cursor;

  // Bottom taskbar. Expect the app list to be in the bottom-left corner.
  taskbar_rect = gfx::Rect(0, kScreenHeight - 30, kScreenWidth, 30);
  cursor = gfx::Point(158, kScreenHeight - 81);
  EXPECT_EQ(gfx::Point(53, kScreenHeight - 133),
            FindAnchorPoint(view_size, display, cursor, taskbar_rect));

  // Top taskbar. Expect the app list to be in the top-left corner.
  taskbar_rect = gfx::Rect(0, 0, kScreenWidth, 30);
  cursor = gfx::Point(158, 81);
  EXPECT_EQ(gfx::Point(53, 133),
            FindAnchorPoint(view_size, display, cursor, taskbar_rect));

  // Left taskbar. Expect the app list to be in the top-left corner.
  taskbar_rect = gfx::Rect(0, 0, 30, kScreenHeight);
  cursor = gfx::Point(81, 158);
  EXPECT_EQ(gfx::Point(83, 103),
            FindAnchorPoint(view_size, display, cursor, taskbar_rect));

  // Right taskbar. Expect the app list to be in the top-right corner.
  taskbar_rect = gfx::Rect(kScreenWidth - 30, 0, 30, kScreenHeight);
  cursor = gfx::Point(kScreenWidth - 81, 158);
  EXPECT_EQ(gfx::Point(kScreenWidth - 83, 103),
            FindAnchorPoint(view_size, display, cursor, taskbar_rect));
}

TEST_F(AppListWinUnitTest, FindAnchorPointMouseOnTaskbar) {
  // Position the app list when the mouse is over the taskbar.
  gfx::Display display;
  display.set_bounds(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
  display.set_work_area(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
  gfx::Size view_size(100, 200);
  gfx::Rect taskbar_rect;
  gfx::Point cursor;

  // Bottom taskbar (mouse well within taskbar). Expect the app list to be at
  // the bottom centered on the mouse X coordinate.
  taskbar_rect = gfx::Rect(0, kScreenHeight - 30, kScreenWidth, 30);
  cursor = gfx::Point(158, kScreenHeight - 20);
  EXPECT_EQ(gfx::Point(158, kScreenHeight - 133),
            FindAnchorPoint(view_size, display, cursor, taskbar_rect));

  // Bottom taskbar (50 pixels outside taskbar; still counts as close enough).
  // Expect the app list to be at the bottom centered on the mouse X coordinate.
  taskbar_rect = gfx::Rect(0, kScreenHeight - 30, kScreenWidth, 30);
  cursor = gfx::Point(158, kScreenHeight - 80);
  EXPECT_EQ(gfx::Point(158, kScreenHeight - 133),
            FindAnchorPoint(view_size, display, cursor, taskbar_rect));

  // Top taskbar. Expect the app list to be at the top centered on the
  // mouse X coordinate.
  taskbar_rect = gfx::Rect(0, 0, kScreenWidth, 30);
  cursor = gfx::Point(158, 80);
  EXPECT_EQ(gfx::Point(158, 133),
            FindAnchorPoint(view_size, display, cursor, taskbar_rect));

  // Left taskbar. Expect the app list to be at the top centered on the
  // mouse X coordinate.
  taskbar_rect = gfx::Rect(0, 0, 30, kScreenHeight);
  cursor = gfx::Point(80, 158);
  EXPECT_EQ(gfx::Point(83, 158),
            FindAnchorPoint(view_size, display, cursor, taskbar_rect));

  // Right taskbar. Expect the app list to be at the bottom centered on the
  // mouse X coordinate.
  taskbar_rect = gfx::Rect(kScreenWidth - 30, 0, 30, kScreenHeight);
  cursor = gfx::Point(kScreenWidth - 80, 158);
  EXPECT_EQ(gfx::Point(kScreenWidth - 83, 158),
            FindAnchorPoint(view_size, display, cursor, taskbar_rect));

  // Bottom taskbar. Mouse near left edge. App list must not go off screen.
  taskbar_rect = gfx::Rect(0, 570, kScreenWidth, 30);
  cursor = gfx::Point(20, kScreenHeight - 20);
  EXPECT_EQ(gfx::Point(53, kScreenHeight - 133),
            FindAnchorPoint(view_size, display, cursor, taskbar_rect));

  // Bottom taskbar. Mouse near right edge. App list must not go off screen.
  taskbar_rect = gfx::Rect(0, 570, kScreenWidth, 30);
  cursor = gfx::Point(kScreenWidth - 20, kScreenHeight - 20);
  EXPECT_EQ(gfx::Point(kScreenWidth - 53, kScreenHeight - 133),
            FindAnchorPoint(view_size, display, cursor, taskbar_rect));
}

TEST_F(AppListWinUnitTest, FindAnchorPointWin8SplitScreen) {
  // Make the work area smaller than the screen, as you would get in Windows 8
  // when there is a split screen and the Desktop is in one side of the screen.
  gfx::Display display;
  display.set_bounds(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
  display.set_work_area(gfx::Rect(200, 0, kScreenWidth - 200, kScreenHeight));
  gfx::Size view_size(100, 200);
  gfx::Rect taskbar_rect;
  gfx::Point cursor;

  // No taskbar. Expect the app list to be in the bottom-left corner of the work
  // area.
  EXPECT_EQ(gfx::Point(253, kScreenHeight - 103),
            FindAnchorPoint(view_size, display, cursor, gfx::Rect()));

  // Bottom taskbar (mouse off-screen to the left). Expect the app list to be
  // in the bottom-left corner of the work area.
  taskbar_rect = gfx::Rect(200, kScreenHeight - 30, kScreenWidth - 200, 30);
  cursor = gfx::Point(158, kScreenHeight - 20);
  EXPECT_EQ(gfx::Point(253, kScreenHeight - 133),
            FindAnchorPoint(view_size, display, cursor, taskbar_rect));
}
