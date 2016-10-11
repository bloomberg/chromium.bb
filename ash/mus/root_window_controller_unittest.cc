// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/root_window_controller.h"

#include "ash/common/test/ash_test.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"

namespace ash {

using RootWindowControllerTest = AshTest;

TEST_F(RootWindowControllerTest, CreateFullscreenWindow) {
  std::unique_ptr<WindowOwner> window_owner = CreateToplevelTestWindow();
  WmWindow* window = window_owner->window();
  window->SetFullscreen();
  WmWindow* root_window = WmShell::Get()->GetPrimaryRootWindow();
  EXPECT_EQ(root_window->GetBounds(), window->GetBounds());
}

}  // namespace ash
