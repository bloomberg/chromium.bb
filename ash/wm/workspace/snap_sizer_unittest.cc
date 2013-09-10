// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/snap_sizer.h"

#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
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

  SnapSizer::SnapWindow(window.get(), SnapSizer::LEFT_EDGE);
  gfx::Rect expected = gfx::Rect(
      kPrimaryDisplayWorkAreaBounds.x(),
      kPrimaryDisplayWorkAreaBounds.y(),
      window->bounds().width(), // No expectation for the width.
      kPrimaryDisplayWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  SnapSizer::SnapWindow(window.get(), SnapSizer::RIGHT_EDGE);
  // The width should not change when a window switches from being snapped to
  // the left edge to being snapped to the right edge.
  expected.set_x(kPrimaryDisplayWorkAreaBounds.right() - expected.width());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // Move the window to the secondary display.
  window->SetBoundsInScreen(gfx::Rect(600, 0, 100, 100),
                            ScreenAsh::GetSecondaryDisplay());

  SnapSizer::SnapWindow(window.get(), SnapSizer::RIGHT_EDGE);
  expected = gfx::Rect(
      kSecondaryDisplayWorkAreaBounds.right() - window->bounds().width(),
      kSecondaryDisplayWorkAreaBounds.y(),
      window->bounds().width(),  // No expectation for the width.
      kSecondaryDisplayWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  SnapSizer::SnapWindow(window.get(), SnapSizer::LEFT_EDGE);
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
  EXPECT_TRUE(ash::wm::CanSnapWindow(window.get()));
  SnapSizer::SnapWindow(window.get(), SnapSizer::RIGHT_EDGE);
  gfx::Rect expected = gfx::Rect(kWorkAreaBounds.x() + 1,
                                 kWorkAreaBounds.y(),
                                 kWorkAreaBounds.width() - 1,
                                 kWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // It should not be possible to snap a window with a maximum size.
  delegate.set_minimum_size(gfx::Size());
  delegate.set_maximum_size(gfx::Size(kWorkAreaBounds.width() - 1, INT_MAX));
  EXPECT_FALSE(ash::wm::CanSnapWindow(window.get()));
}

}  // namespace ash
