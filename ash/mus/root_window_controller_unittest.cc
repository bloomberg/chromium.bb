// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/root_window_controller.h"

#include "ash/common/test/ash_test.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/mus/test/wm_test_base.h"

namespace ash {

using RootWindowControllerTest = AshTest;

TEST_F(RootWindowControllerTest, CreateFullscreenWindow) {
  std::unique_ptr<WindowOwner> window_owner = CreateToplevelTestWindow();
  WmWindow* window = window_owner->window();
  window->SetFullscreen();
  WmWindow* root_window = WmShell::Get()->GetPrimaryRootWindow();
  EXPECT_EQ(root_window->GetBounds(), window->GetBounds());
}

using RootWindowControllerWmTest = mus::WmTestBase;

TEST_F(RootWindowControllerWmTest, IsWindowShownInCorrectDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("400x400,400x400");
  EXPECT_NE(GetPrimaryDisplay().id(), GetSecondaryDisplay().id());

  ui::Window* window_primary_display =
      CreateFullscreenTestWindow(GetPrimaryDisplay().id());
  ui::Window* window_secondary_display =
      CreateFullscreenTestWindow(GetSecondaryDisplay().id());

  DCHECK(window_primary_display);
  DCHECK(window_secondary_display);

  EXPECT_EQ(window_primary_display->display_id(), GetPrimaryDisplay().id());
  EXPECT_EQ(window_secondary_display->display_id(), GetSecondaryDisplay().id());
}

}  // namespace ash
