// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_state.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"

namespace ash {
namespace wm {

typedef test::AshTestBase WindowStateTest;

// Test that a window gets properly snapped to the display's edges in a
// multi monitor environment.
TEST_F(WindowStateTest, SnapWindowBasic) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("0+0-500x400, 0+500-600x400");
  const gfx::Rect kPrimaryDisplayWorkAreaBounds =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();
  const gfx::Rect kSecondaryDisplayWorkAreaBounds =
      ScreenUtil::GetSecondaryDisplay().work_area();

  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->SnapLeftWithDefaultWidth();
  gfx::Rect expected = gfx::Rect(
      kPrimaryDisplayWorkAreaBounds.x(),
      kPrimaryDisplayWorkAreaBounds.y(),
      kPrimaryDisplayWorkAreaBounds.width() / 2,
      kPrimaryDisplayWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  window_state->SnapRightWithDefaultWidth();
  expected.set_x(kPrimaryDisplayWorkAreaBounds.right() - expected.width());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // Move the window to the secondary display.
  window->SetBoundsInScreen(gfx::Rect(600, 0, 100, 100),
                            ScreenUtil::GetSecondaryDisplay());

  window_state->SnapRightWithDefaultWidth();
  expected = gfx::Rect(
      kSecondaryDisplayWorkAreaBounds.x() +
          kSecondaryDisplayWorkAreaBounds.width() / 2,
      kSecondaryDisplayWorkAreaBounds.y(),
      kSecondaryDisplayWorkAreaBounds.width() / 2,
      kSecondaryDisplayWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  window_state->SnapLeftWithDefaultWidth();
  expected.set_x(kSecondaryDisplayWorkAreaBounds.x());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());
}

// Test how the minimum and maximum size specified by the aura::WindowDelegate
// affect snapping.
TEST_F(WindowStateTest, SnapWindowMinimumSize) {
  if (!SupportsHostWindowResize())
    return;

  UpdateDisplay("0+0-600x900");
  const gfx::Rect kWorkAreaBounds =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();

  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(0, 100, kWorkAreaBounds.width() - 1, 100)));

  // It should be possible to snap a window with a minimum size.
  delegate.set_minimum_size(gfx::Size(kWorkAreaBounds.width() - 1, 0));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  EXPECT_TRUE(window_state->CanSnap());
  window_state->SnapRightWithDefaultWidth();
  gfx::Rect expected = gfx::Rect(kWorkAreaBounds.x() + 1,
                                 kWorkAreaBounds.y(),
                                 kWorkAreaBounds.width() - 1,
                                 kWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // It should not be possible to snap a window with a maximum size.
  delegate.set_minimum_size(gfx::Size());
  delegate.set_maximum_size(gfx::Size(kWorkAreaBounds.width() - 1, INT_MAX));
  EXPECT_FALSE(window_state->CanSnap());
}

// Test that setting the bounds of a snapped window keeps its snapped.
TEST_F(WindowStateTest, SnapWindowSetBounds) {
  if (!SupportsHostWindowResize())
    return;

  UpdateDisplay("0+0-900x600");
  const gfx::Rect kWorkAreaBounds =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();

  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->SnapLeftWithDefaultWidth();
  EXPECT_EQ(SHOW_TYPE_LEFT_SNAPPED, window_state->window_show_type());
  gfx::Rect expected = gfx::Rect(kWorkAreaBounds.x(),
                                 kWorkAreaBounds.y(),
                                 kWorkAreaBounds.width() / 2,
                                 kWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // Snapped windows can have any width.
  expected.set_width(500);
  window->SetBounds(gfx::Rect(10, 10, 500, 300));
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());
  EXPECT_EQ(SHOW_TYPE_LEFT_SNAPPED, window_state->window_show_type());
}

// Test that snapping left/right preserves the restore bounds.
TEST_F(WindowStateTest, RestoreBounds) {
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());

  EXPECT_TRUE(window_state->IsNormalShowType());

  // 1) Start with restored window with restore bounds set.
  gfx::Rect restore_bounds = window->GetBoundsInScreen();
  restore_bounds.set_width(restore_bounds.width() + 1);
  window_state->SetRestoreBoundsInScreen(restore_bounds);
  window_state->SnapLeftWithDefaultWidth();
  window_state->SnapRightWithDefaultWidth();
  EXPECT_NE(restore_bounds.ToString(), window->GetBoundsInScreen().ToString());
  EXPECT_EQ(restore_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());
  window_state->Restore();
  EXPECT_EQ(restore_bounds.ToString(), window->GetBoundsInScreen().ToString());

  // 2) Start with restored bounds set as a result of maximizing the window.
  window_state->Maximize();
  gfx::Rect maximized_bounds = window->GetBoundsInScreen();
  EXPECT_NE(maximized_bounds.ToString(), restore_bounds.ToString());
  EXPECT_EQ(restore_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());

  window_state->SnapLeftWithDefaultWidth();
  EXPECT_NE(restore_bounds.ToString(), window->GetBoundsInScreen().ToString());
  EXPECT_NE(maximized_bounds.ToString(),
            window->GetBoundsInScreen().ToString());
  EXPECT_EQ(restore_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(restore_bounds.ToString(), window->GetBoundsInScreen().ToString());
}

// Test that maximizing an auto managed window, then snapping it puts the window
// at the snapped bounds and not at the auto-managed (centered) bounds.
TEST_F(WindowStateTest, AutoManaged) {
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->set_window_position_managed(true);
  window->Hide();
  window->SetBounds(gfx::Rect(100, 100, 100, 100));
  window->Show();

  window_state->Maximize();
  window_state->SnapRightWithDefaultWidth();

  const gfx::Rect kWorkAreaBounds =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect expected_snapped_bounds(
      kWorkAreaBounds.x() + kWorkAreaBounds.width() / 2,
      kWorkAreaBounds.y(),
      kWorkAreaBounds.width() / 2,
      kWorkAreaBounds.height());
  EXPECT_EQ(expected_snapped_bounds.ToString(),
            window->GetBoundsInScreen().ToString());

  // The window should still be auto managed despite being right maximized.
  EXPECT_TRUE(window_state->window_position_managed());
}

}  // namespace wm
}  // namespace ash
