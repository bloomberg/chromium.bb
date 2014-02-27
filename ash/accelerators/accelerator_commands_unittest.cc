// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window.h"

// Note: The unit tests for |ToggleMaximized()| and
// |ToggleFullscreen()| are in
// chrome/browser/ui/ash/accelerator_commands_browsertests.cc.
// because they depends on chrome implementation of
// |ash::wm::WindowStateDelegate|.

namespace ash {
namespace accelerators {

typedef test::AshTestBase AcceleratorCommandsTest;

TEST_F(AcceleratorCommandsTest, ToggleMinimized) {
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->Activate();

  ToggleMinimized();
  EXPECT_TRUE(window_state->IsMinimized());
  EXPECT_FALSE(window_state->IsNormalStateType());

  ToggleMinimized();
  EXPECT_FALSE(window_state->IsMinimized());
  EXPECT_TRUE(window_state->IsNormalStateType());
}

}  // namespace accelerators
}  // namespace ash
