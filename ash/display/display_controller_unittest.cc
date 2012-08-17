// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/display_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace test {
namespace {

gfx::Display GetPrimaryDisplay() {
  return gfx::Screen::GetDisplayNearestWindow(
      Shell::GetAllRootWindows()[0]);
}

gfx::Display GetSecondaryDisplay() {
  return gfx::Screen::GetDisplayNearestWindow(
      Shell::GetAllRootWindows()[1]);
}

}  // namespace

typedef test::AshTestBase DisplayControllerTest;

#if defined(OS_WIN)
// TOD(oshima): Windows creates a window with smaller client area.
// Fix this and enable tests.
#define MAYBE_SecondaryDisplayLayout DISABLED_SecondaryDisplayLayout
#else
#define MAYBE_SecondaryDisplayLayout SecondaryDisplayLayout
#endif

TEST_F(DisplayControllerTest, MAYBE_SecondaryDisplayLayout) {
  UpdateDisplay("500x500,400x400");
  gfx::Display* secondary_display =
      aura::Env::GetInstance()->display_manager()->GetDisplayAt(1);
  gfx::Insets insets(5, 5, 5, 5);
  secondary_display->UpdateWorkAreaFromInsets(insets);

  // Default layout is LEFT.
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("500,0 400x400", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("505,5 390x390", GetSecondaryDisplay().work_area().ToString());

  // Layout the secondary display to the bottom of the primary.
  Shell::GetInstance()->display_controller()->SetSecondaryDisplayLayout(
      internal::DisplayController::BOTTOM);
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,500 400x400", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("5,505 390x390", GetSecondaryDisplay().work_area().ToString());

  // Layout the secondary display to the left of the primary.
  Shell::GetInstance()->display_controller()->SetSecondaryDisplayLayout(
      internal::DisplayController::LEFT);
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("-400,0 400x400", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("-395,5 390x390", GetSecondaryDisplay().work_area().ToString());

  // Layout the secondary display to the top of the primary.
  Shell::GetInstance()->display_controller()->SetSecondaryDisplayLayout(
      internal::DisplayController::TOP);
  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,-400 400x400", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("5,-395 390x390", GetSecondaryDisplay().work_area().ToString());
}

// TODO(oshima,erg): I suspect this test is now failing because I've changed
// the timing of the RootWindow::Show to be synchronous. If true, this test has
// always been incorrect, but is now visibly broken now that we're processing
// X11 configuration events while waiting for the MapNotify.
TEST_F(DisplayControllerTest, DISABLED_BoundsUpdated) {
  Shell::GetInstance()->display_controller()->SetSecondaryDisplayLayout(
      internal::DisplayController::BOTTOM);
  UpdateDisplay("500x500,400x400");
  gfx::Display* secondary_display =
      aura::Env::GetInstance()->display_manager()->GetDisplayAt(1);
  gfx::Insets insets(5, 5, 5, 5);
  secondary_display->UpdateWorkAreaFromInsets(insets);

  EXPECT_EQ("0,0 500x500", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,500 400x400", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("5,505 390x390", GetSecondaryDisplay().work_area().ToString());

  UpdateDisplay("600x600,400x400");
  EXPECT_EQ("0,0 600x600", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,600 400x400", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("5,605 390x390", GetSecondaryDisplay().work_area().ToString());

  UpdateDisplay("600x600,500x500");
  EXPECT_EQ("0,0 600x600", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,600 500x500", GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("5,605 490x490", GetSecondaryDisplay().work_area().ToString());

  UpdateDisplay("600x600");
  EXPECT_EQ("0,0 600x600", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ(1, gfx::Screen::GetNumDisplays());

  UpdateDisplay("700x700,1000x1000");
  ASSERT_EQ(2, gfx::Screen::GetNumDisplays());
  EXPECT_EQ("0,0 700x700", GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("0,700 1000x1000", GetSecondaryDisplay().bounds().ToString());
}

// Verifies if the mouse pointer correctly moves to another display when there
// are two displays.
TEST_F(DisplayControllerTest, WarpMouse) {
  UpdateDisplay("500x500,500x500");

  ash::internal::DisplayController* controller =
      Shell::GetInstance()->display_controller();
  EXPECT_EQ(internal::DisplayController::RIGHT,
            controller->secondary_display_layout());

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  bool is_warped = controller->WarpMouseCursorIfNecessary(root_windows[0],
                                                          gfx::Point(11, 11));
  EXPECT_FALSE(is_warped);
  is_warped = controller->WarpMouseCursorIfNecessary(root_windows[1],
                                                     gfx::Point(11, 11));
  EXPECT_FALSE(is_warped);

  // Touch the right edge of the primary root window. Pointer should warp.
  is_warped = controller->WarpMouseCursorIfNecessary(root_windows[0],
                                                     gfx::Point(499, 11));
  EXPECT_TRUE(is_warped);
  EXPECT_EQ("501,11",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the secondary root window. Pointer should warp.
  is_warped = controller->WarpMouseCursorIfNecessary(root_windows[1],
                                                     gfx::Point(0, 11));
  EXPECT_TRUE(is_warped);
  EXPECT_EQ("498,11",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the primary root window.
  is_warped = controller->WarpMouseCursorIfNecessary(root_windows[0],
                                                     gfx::Point(0, 11));
  EXPECT_FALSE(is_warped);
  // Touch the top edge of the primary root window.
  is_warped = controller->WarpMouseCursorIfNecessary(root_windows[0],
                                                     gfx::Point(11, 0));
  EXPECT_FALSE(is_warped);
  // Touch the bottom edge of the primary root window.
  is_warped = controller->WarpMouseCursorIfNecessary(root_windows[0],
                                                     gfx::Point(11, 499));
  EXPECT_FALSE(is_warped);
  // Touch the right edge of the secondary root window.
  is_warped = controller->WarpMouseCursorIfNecessary(root_windows[1],
                                                     gfx::Point(499, 11));
  EXPECT_FALSE(is_warped);
  // Touch the top edge of the secondary root window.
  is_warped = controller->WarpMouseCursorIfNecessary(root_windows[1],
                                                     gfx::Point(11, 0));
  EXPECT_FALSE(is_warped);
  // Touch the bottom edge of the secondary root window.
  is_warped = controller->WarpMouseCursorIfNecessary(root_windows[1],
                                                     gfx::Point(11, 499));
  EXPECT_FALSE(is_warped);
}

// Verifies if the mouse pointer correctly moves to another display even when
// two displays are not the same size.
TEST_F(DisplayControllerTest, WarpMouseDifferentSizeDisplays) {
  UpdateDisplay("500x500,600x600");  // the second one is larger.

  ash::internal::DisplayController* controller =
      Shell::GetInstance()->display_controller();
  EXPECT_EQ(internal::DisplayController::RIGHT,
            controller->secondary_display_layout());

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  aura::Env::GetInstance()->SetLastMouseLocation(*root_windows[1],
                                                 gfx::Point(123, 123));

  // Touch the left edge of the secondary root window. Pointer should NOT warp
  // because 1px left of (0, 500) is outside the primary root window.
  bool is_warped = controller->WarpMouseCursorIfNecessary(root_windows[1],
                                                          gfx::Point(0, 500));
  EXPECT_FALSE(is_warped);
  EXPECT_EQ("623,123",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the secondary root window. Pointer should warp
  // because 1px left of (0, 499) is inside the primary root window.
  is_warped = controller->WarpMouseCursorIfNecessary(root_windows[1],
                                                     gfx::Point(0, 499));
  EXPECT_TRUE(is_warped);
  EXPECT_EQ("498,499",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());
}

// Verifies if DisplayController::dont_warp_mouse() works as expected.
TEST_F(DisplayControllerTest, SetUnsetDontWarpMousedFlag) {
  UpdateDisplay("500x500,500x500");

  ash::internal::DisplayController* controller =
      Shell::GetInstance()->display_controller();

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  aura::Env::GetInstance()->SetLastMouseLocation(*root_windows[0],
                                                 gfx::Point(1, 1));

  controller->set_dont_warp_mouse(true);
  bool is_warped = controller->WarpMouseCursorIfNecessary(root_windows[0],
                                                          gfx::Point(499, 11));
  EXPECT_FALSE(is_warped);
  EXPECT_EQ("1,1",
            aura::Env::GetInstance()->last_mouse_location().ToString());

  controller->set_dont_warp_mouse(false);
  is_warped = controller->WarpMouseCursorIfNecessary(root_windows[0],
                                                     gfx::Point(499, 11));
  EXPECT_TRUE(is_warped);
  EXPECT_EQ("501,11",
            aura::Env::GetInstance()->last_mouse_location().ToString());
}

}  // namespace test
}  // namespace ash
