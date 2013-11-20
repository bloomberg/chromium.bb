// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/snap_sizer.h"

#include "ash/ash_switches.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"

namespace ash {

typedef test::AshTestBase SnapSizerTest;

using internal::SnapSizer;

// Test that a window gets properly snapped to the display's edges in a
// multi monitor environment.
TEST_F(SnapSizerTest, MultipleDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("0+0-500x400, 0+500-600x400");
  const gfx::Rect kPrimaryDisplayWorkAreaBounds =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();
  const gfx::Rect kSecondaryDisplayWorkAreaBounds =
      ScreenAsh::GetSecondaryDisplay().work_area();

  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  SnapSizer::SnapWindow(window_state, SnapSizer::LEFT_EDGE);
  gfx::Rect expected = gfx::Rect(
      kPrimaryDisplayWorkAreaBounds.x(),
      kPrimaryDisplayWorkAreaBounds.y(),
      window->bounds().width(), // No expectation for the width.
      kPrimaryDisplayWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  SnapSizer::SnapWindow(window_state, SnapSizer::RIGHT_EDGE);
  // The width should not change when a window switches from being snapped to
  // the left edge to being snapped to the right edge.
  expected.set_x(kPrimaryDisplayWorkAreaBounds.right() - expected.width());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // Move the window to the secondary display.
  window->SetBoundsInScreen(gfx::Rect(600, 0, 100, 100),
                            ScreenAsh::GetSecondaryDisplay());

  SnapSizer::SnapWindow(window_state, SnapSizer::RIGHT_EDGE);
  expected = gfx::Rect(
      kSecondaryDisplayWorkAreaBounds.right() - window->bounds().width(),
      kSecondaryDisplayWorkAreaBounds.y(),
      window->bounds().width(),  // No expectation for the width.
      kSecondaryDisplayWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  SnapSizer::SnapWindow(window_state, SnapSizer::LEFT_EDGE);
  // The width should not change when a window switches from being snapped to
  // the right edge to being snapped to the left edge.
  expected.set_x(kSecondaryDisplayWorkAreaBounds.x());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());
}

// Test how the minimum and maximum size specified by the aura::WindowDelegate
// affect snapping.
TEST_F(SnapSizerTest, MinimumSize) {
  if (!SupportsHostWindowResize())
    return;

  UpdateDisplay("0+0-600x800");
  const gfx::Rect kWorkAreaBounds =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();

  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(0, 100, kWorkAreaBounds.width() - 1, 100)));

  // It should be possible to snap a window with a minimum size.
  delegate.set_minimum_size(gfx::Size(kWorkAreaBounds.width() - 1, 0));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  EXPECT_TRUE(window_state->CanSnap());
  SnapSizer::SnapWindow(window_state, SnapSizer::RIGHT_EDGE);
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

// Test that repeatedly calling SnapSizer::SnapWindow() when the
// --ash-multiple-snap-window-widths flag is set steps through the ideal widths
// in descending order as well as 90% and 50% of the work area's width.
TEST_F(SnapSizerTest, StepThroughSizes) {
  if (!SupportsHostWindowResize())
    return;
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshMultipleSnapWindowWidths);

  UpdateDisplay("1024x800");
  const gfx::Rect kWorkAreaBounds =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();

  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());

  // Make sure that the work area is the size we expect it to be.
  EXPECT_GT(kWorkAreaBounds.width() * 0.9, 768);

  // The first width should be 1024 * 0.9 because the larger ideal widths
  // (1280, 1024) > 1024 * 0.9.
  SnapSizer::SnapWindow(window_state, SnapSizer::LEFT_EDGE);
  gfx::Rect expected = gfx::Rect(kWorkAreaBounds.x(),
                                 kWorkAreaBounds.y(),
                                 kWorkAreaBounds.width() * 0.9,
                                 kWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  SnapSizer::SnapWindow(window_state, SnapSizer::LEFT_EDGE);
  expected.set_width(768);
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  SnapSizer::SnapWindow(window_state, SnapSizer::LEFT_EDGE);
  expected.set_width(640);
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  SnapSizer::SnapWindow(window_state, SnapSizer::LEFT_EDGE);
  expected.set_width(kWorkAreaBounds.width() * 0.5);
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // Wrap around.
  SnapSizer::SnapWindow(window_state, SnapSizer::LEFT_EDGE);
  expected.set_width(kWorkAreaBounds.width() * 0.9);
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // Repeat the test snapping right.
  SnapSizer::SnapWindow(window_state, SnapSizer::RIGHT_EDGE);
  expected.set_width(kWorkAreaBounds.width() * 0.9);
  expected.set_x(kWorkAreaBounds.right() - expected.width());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  SnapSizer::SnapWindow(window_state, SnapSizer::RIGHT_EDGE);
  expected.set_width(768);
  expected.set_x(kWorkAreaBounds.right() - expected.width());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  SnapSizer::SnapWindow(window_state, SnapSizer::RIGHT_EDGE);
  expected.set_width(640);
  expected.set_x(kWorkAreaBounds.right() - expected.width());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  SnapSizer::SnapWindow(window_state, SnapSizer::RIGHT_EDGE);
  expected.set_width(kWorkAreaBounds.width() * 0.5);
  expected.set_x(kWorkAreaBounds.right() - expected.width());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // Wrap around.
  SnapSizer::SnapWindow(window_state, SnapSizer::RIGHT_EDGE);
  expected.set_width(kWorkAreaBounds.width() * 0.9);
  expected.set_x(kWorkAreaBounds.right() - expected.width());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());
}

// Test that a SnapSizer's target bounds always start from the widest possible
// bounds and that calling Update() steps through the ideal widths in descending
// order as well as 90% and 50% of the work area's width.
TEST_F(SnapSizerTest, Update) {
  if (!SupportsHostWindowResize())
    return;
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshMultipleSnapWindowWidths);
  UpdateDisplay("1024x800");
  const gfx::Rect kWorkAreaBounds =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();

  // Make sure that the work area is the size we expect it to be.
  EXPECT_GT(kWorkAreaBounds.width() * 0.9, 768);

  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());

  SnapSizer::SnapWindow(window_state, SnapSizer::LEFT_EDGE);
  SnapSizer::SnapWindow(window_state, SnapSizer::LEFT_EDGE);
  gfx::Rect expected = gfx::Rect(kWorkAreaBounds.x(),
                                 kWorkAreaBounds.y(),
                                 768,
                                 kWorkAreaBounds.height());

  // The SnapSizer's target bounds should always start from the widest bounds
  // (instead of the bounds with the next ideal width).
  SnapSizer sizer(window_state, gfx::Point(800, 0), SnapSizer::LEFT_EDGE,
      SnapSizer::OTHER_INPUT);
  sizer.SnapWindowToTargetBounds();
  expected.set_width(kWorkAreaBounds.width() * .9);
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  sizer.Update(gfx::Point(600, 0));
  sizer.SnapWindowToTargetBounds();
  expected.set_width(768);
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  sizer.Update(gfx::Point(400, 0));
  sizer.SnapWindowToTargetBounds();
  expected.set_width(640);
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  sizer.Update(gfx::Point(200, 0));
  sizer.SnapWindowToTargetBounds();
  expected.set_width(kWorkAreaBounds.width() * 0.5);
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // Do not wrap around.
  sizer.Update(gfx::Point(0, 0));
  sizer.SnapWindowToTargetBounds();
  expected.set_width(kWorkAreaBounds.width() * 0.5);
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());
}

// Tests the SnapSizer's target bounds when resizing is disabled and the
// --ash-multiple-snap-window-widths flag is set.
TEST_F(SnapSizerTest, Default) {
  if (!SupportsHostWindowResize())
    return;
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshMultipleSnapWindowWidths);

  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  SnapSizer sizer(wm::GetWindowState(window.get()), gfx::Point(),
                  SnapSizer::LEFT_EDGE, SnapSizer::OTHER_INPUT);

  // For small workspace widths, we should snap to 90% of the workspace width
  // because it is the largest width the window can snap to.
  UpdateDisplay("0+0-800x600");
  sizer.SelectDefaultSizeAndDisableResize();

  gfx::Rect work_area =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect expected(work_area);
  expected.set_width(work_area.width() * 0.9);
  EXPECT_EQ(expected.ToString(),
            ScreenAsh::ConvertRectToScreen(window->parent(),
                                           sizer.target_bounds()).ToString());

  // If the largest width the window can snap to is between 1024 and 1280, we
  // should snap to 1024.
  UpdateDisplay("0+0-1280x800");
  sizer.SelectDefaultSizeAndDisableResize();
  sizer.SnapWindowToTargetBounds();
  EXPECT_EQ(1024, window->bounds().width());

  // We should snap to a width of 50% of the work area if it is the largest
  // width the window can snap to.
  UpdateDisplay("0+0-2560x1080");
  work_area = ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();
  sizer.SelectDefaultSizeAndDisableResize();
  sizer.SnapWindowToTargetBounds();
  EXPECT_EQ(work_area.width() / 2, window->bounds().width());
}

// Test that the window only snaps to 50% of the work area width when the
// --ash-multiple-snap-window-widths flag is not set.
TEST_F(SnapSizerTest, SingleSnapWindowWidth) {
  if (!SupportsHostWindowResize())
    return;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshMultipleSnapWindowWidths)) {
    return;
  }

  UpdateDisplay("0+0-800x600");
  const gfx::Rect kWorkAreaBounds =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();

  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  SnapSizer::SnapWindow(window_state, SnapSizer::LEFT_EDGE);
  gfx::Rect expected = gfx::Rect(kWorkAreaBounds.x(),
                                 kWorkAreaBounds.y(),
                                 kWorkAreaBounds.width() / 2,
                                 kWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // Because a window can only be snapped to one size when using the alternate
  // caption button style, a second call to SnapSizer::SnapWindow() should have
  // no effect.
  SnapSizer::SnapWindow(window_state, SnapSizer::LEFT_EDGE);
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // It should still be possible to switch a window from being snapped to the
  // left edge to being snapped to the right edge.
  SnapSizer::SnapWindow(window_state, SnapSizer::RIGHT_EDGE);
  expected.set_x(kWorkAreaBounds.right() - expected.width());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // If resizing is disabled, the window should be snapped to 50% too.
  SnapSizer sizer1(window_state, gfx::Point(), SnapSizer::RIGHT_EDGE,
      SnapSizer::OTHER_INPUT);
  sizer1.SelectDefaultSizeAndDisableResize();
  sizer1.SnapWindowToTargetBounds();
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // Snapping to a SnapSizer's initial bounds snaps to 50% too.
  SnapSizer sizer2(window_state, gfx::Point(), SnapSizer::LEFT_EDGE,
      SnapSizer::OTHER_INPUT);
  sizer2.SnapWindowToTargetBounds();
  expected.set_x(kWorkAreaBounds.x());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());
}

// Test that snapping left/right preserves the restore bounds.
TEST_F(SnapSizerTest, RestoreBounds) {
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());

  EXPECT_TRUE(window_state->IsNormalShowState());

  // 1) Start with restored window with restore bounds set.
  gfx::Rect restore_bounds = window->GetBoundsInScreen();
  restore_bounds.set_width(restore_bounds.width() + 1);
  window_state->SetRestoreBoundsInScreen(restore_bounds);
  SnapSizer::SnapWindow(window_state, SnapSizer::LEFT_EDGE);
  SnapSizer::SnapWindow(window_state, SnapSizer::RIGHT_EDGE);
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

  SnapSizer::SnapWindow(window_state, SnapSizer::LEFT_EDGE);
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
TEST_F(SnapSizerTest, AutoManaged) {
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->set_window_position_managed(true);
  window->Hide();
  window->SetBounds(gfx::Rect(100, 100, 100, 100));
  window->Show();

  window_state->Maximize();
  SnapSizer::SnapWindow(window_state, SnapSizer::RIGHT_EDGE);

  const gfx::Rect kWorkAreaBounds =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect expected_snapped_bounds(
      kWorkAreaBounds.right() - window->bounds().width(),
      kWorkAreaBounds.y(),
      window->bounds().width(), // No expectation for the width.
      kWorkAreaBounds.height());
  EXPECT_EQ(expected_snapped_bounds.ToString(),
            window->GetBoundsInScreen().ToString());

  // The window should still be auto managed despite being right maximized.
  EXPECT_TRUE(window_state->window_position_managed());
}

}  // namespace ash
