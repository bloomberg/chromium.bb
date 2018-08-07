// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scoped_root_window_for_new_windows.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"

namespace ash {

using ScreenAshTest = AshTestBase;

// Tests that ScreenAsh::GetWindowAtScreenPoint() returns the correct window on
// the correct display.
TEST_F(ScreenAshTest, TestGetWindowAtScreenPoint) {
  UpdateDisplay("200x200,400x400");

  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> win1(CreateTestWindowInShellWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 200, 200)));

  std::unique_ptr<aura::Window> win2(CreateTestWindowInShellWithDelegate(
      &delegate, 1, gfx::Rect(200, 200, 100, 100)));

  ASSERT_NE(win1->GetRootWindow(), win2->GetRootWindow());

  EXPECT_EQ(win1.get(), display::Screen::GetScreen()->GetWindowAtScreenPoint(
                            gfx::Point(50, 60)));
  EXPECT_EQ(win2.get(), display::Screen::GetScreen()->GetWindowAtScreenPoint(
                            gfx::Point(250, 260)));
}

TEST_F(ScreenAshTest, GetDisplayForNewWindows) {
  UpdateDisplay("200x200,400x400");
  display::Screen* screen = display::Screen::GetScreen();
  const std::vector<display::Display> displays = screen->GetAllDisplays();
  ASSERT_EQ(2u, displays.size());

  // The display for new windows defaults to primary display.
  EXPECT_EQ(displays[0].id(), screen->GetDisplayForNewWindows().id());

  // The display for new windows is updated when the root window for new windows
  // changes.
  ScopedRootWindowForNewWindows scoped_root(Shell::GetAllRootWindows()[1]);
  EXPECT_EQ(displays[1].id(), screen->GetDisplayForNewWindows().id());
}

}  // namespace ash
