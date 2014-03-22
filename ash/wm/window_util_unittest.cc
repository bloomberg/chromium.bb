// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_util.h"

#include "ash/screen_util.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

std::string GetAdjustedBounds(const gfx::Rect& visible,
                              gfx::Rect to_be_adjusted) {
  wm::AdjustBoundsToEnsureMinimumWindowVisibility(visible, &to_be_adjusted);
  return to_be_adjusted.ToString();
}

}

typedef test::AshTestBase WindowUtilTest;

TEST_F(WindowUtilTest, CenterWindow) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("500x400, 600x400");
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(12, 20, 100, 100)));

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  EXPECT_FALSE(window_state->bounds_changed_by_user());

  wm::CenterWindow(window.get());
  // Centring window is considered as a user's action.
  EXPECT_TRUE(window_state->bounds_changed_by_user());
  EXPECT_EQ("200,126 100x100", window->bounds().ToString());
  EXPECT_EQ("200,126 100x100", window->GetBoundsInScreen().ToString());
  window->SetBoundsInScreen(gfx::Rect(600, 0, 100, 100),
                            ScreenUtil::GetSecondaryDisplay());
  wm::CenterWindow(window.get());
  EXPECT_EQ("250,126 100x100", window->bounds().ToString());
  EXPECT_EQ("750,126 100x100", window->GetBoundsInScreen().ToString());
}

TEST_F(WindowUtilTest, AdjustBoundsToEnsureMinimumVisibility) {
  const gfx::Rect visible_bounds(0, 0, 100, 100);

  EXPECT_EQ("0,0 90x90",
            GetAdjustedBounds(visible_bounds, gfx::Rect(0, 0, 90, 90)));
  EXPECT_EQ("0,0 100x100",
            GetAdjustedBounds(visible_bounds, gfx::Rect(0, 0, 150, 150)));
  EXPECT_EQ("-50,0 100x100",
            GetAdjustedBounds(visible_bounds, gfx::Rect(-50, -50, 150, 150)));
  EXPECT_EQ("-90,10 100x100",
            GetAdjustedBounds(visible_bounds, gfx::Rect(-100, 10, 150, 150)));
  EXPECT_EQ("90,90 100x100",
            GetAdjustedBounds(visible_bounds, gfx::Rect(100, 100, 150, 150)));

  const gfx::Rect visible_bounds_right(200, 50, 100, 100);

  EXPECT_EQ(
      "210,60 90x90",
      GetAdjustedBounds(visible_bounds_right, gfx::Rect(210, 60, 90, 90)));
  EXPECT_EQ(
      "210,60 100x100",
      GetAdjustedBounds(visible_bounds_right, gfx::Rect(210, 60, 150, 150)));
  EXPECT_EQ(
      "110,50 100x100",
      GetAdjustedBounds(visible_bounds_right, gfx::Rect(0, 0, 150, 150)));
  EXPECT_EQ(
      "290,50 100x100",
      GetAdjustedBounds(visible_bounds_right, gfx::Rect(300, 20, 150, 150)));
  EXPECT_EQ(
      "110,140 100x100",
      GetAdjustedBounds(visible_bounds_right, gfx::Rect(-100, 150, 150, 150)));

  const gfx::Rect visible_bounds_left(-200, -50, 100, 100);
  EXPECT_EQ(
      "-190,-40 90x90",
      GetAdjustedBounds(visible_bounds_left, gfx::Rect(-190, -40, 90, 90)));
  EXPECT_EQ(
      "-190,-40 100x100",
      GetAdjustedBounds(visible_bounds_left, gfx::Rect(-190, -40, 150, 150)));
  EXPECT_EQ(
      "-250,-40 100x100",
      GetAdjustedBounds(visible_bounds_left, gfx::Rect(-250, -40, 150, 150)));
  EXPECT_EQ(
      "-290,-50 100x100",
      GetAdjustedBounds(visible_bounds_left, gfx::Rect(-400, -60, 150, 150)));
  EXPECT_EQ(
      "-110,0 100x100",
      GetAdjustedBounds(visible_bounds_left, gfx::Rect(0, 0, 150, 150)));
}

}  // namespace ash
