// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/virtual_keyboard_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {
namespace test {

typedef AshTestBase VirtualKeyboardControllerTest;

// Tests that the onscreen keyboard becomes enabled when maximize mode is
// enabled.
TEST_F(VirtualKeyboardControllerTest, EnabledDuringMaximizeMode) {
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
}

}  // namespace test
}  // namespace ash
