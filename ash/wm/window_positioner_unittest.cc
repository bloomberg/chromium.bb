// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_positioner.h"

#include "ash/shell.h"
#include "ash/shell/toplevel_window.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

typedef test::AshTestBase WindowPositionerTest;

TEST_F(WindowPositionerTest, OpenMaximizedWindowOnSecondDisplay) {
  if (!SupportsMultipleDisplays())
    return;
  UpdateDisplay("400x400,500x500");
  Shell::GetInstance()->set_target_root_window(
      Shell::GetAllRootWindows()[1]);
  shell::ToplevelWindow::CreateParams params;
  params.can_resize = true;
  params.can_maximize = true;
  views::Widget* widget =
      shell::ToplevelWindow::CreateToplevelWindow(params);
  EXPECT_EQ("400,0 500x453", widget->GetWindowBoundsInScreen().ToString());
}

TEST_F(WindowPositionerTest, OpenDefaultWindowOnSecondDisplay) {
  if (!SupportsMultipleDisplays())
    return;
  UpdateDisplay("400x400,1400x900");
  aura::Window* second_root_window = Shell::GetAllRootWindows()[1];
  Shell::GetInstance()->set_target_root_window(
      second_root_window);
  shell::ToplevelWindow::CreateParams params;
  params.can_resize = true;
  params.can_maximize = true;
  views::Widget* widget =
      shell::ToplevelWindow::CreateToplevelWindow(params);
  gfx::Rect bounds = widget->GetWindowBoundsInScreen();
  // The window should be in the 2nd display with the default size.
  EXPECT_EQ("300x300", bounds.size().ToString());
  EXPECT_TRUE(Shell::GetScreen()->GetDisplayNearestWindow(
      second_root_window).bounds().Contains(bounds));
}

}  // namespace
