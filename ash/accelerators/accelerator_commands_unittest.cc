// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"

namespace ash {
namespace accelerators {

typedef test::AshTestBase AcceleratorCommandsTest;

TEST_F(AcceleratorCommandsTest, ToggleMinimized) {
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  wm::ActivateWindow(window.get());

  ToggleMinimized();
  EXPECT_TRUE(wm::IsWindowMinimized(window.get()));
  EXPECT_FALSE(wm::IsWindowNormal(window.get()));

  ToggleMinimized();
  EXPECT_FALSE(wm::IsWindowMinimized(window.get()));
  EXPECT_TRUE(wm::IsWindowNormal(window.get()));
}

}  // namespace accelerators
}  // namespace ash
