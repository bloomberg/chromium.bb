// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/virtual_keyboard_window_controller.h"

#include "ash/ash_switches.h"
#include "ash/display/display_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/command_line.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {
namespace test {

class VirtualKeyboardWindowControllerTest : public AshTestBase {
 public:
  VirtualKeyboardWindowControllerTest()
      : virtual_keyboard_window_controller_(NULL) {}
  virtual ~VirtualKeyboardWindowControllerTest() {}

  void set_virtual_keyboard_window_controller(
      VirtualKeyboardWindowController* controller) {
    virtual_keyboard_window_controller_ = controller;
  }

  RootWindowController* root_window_controller() {
    return virtual_keyboard_window_controller_->
        root_window_controller_for_test();
  }

 private:
  VirtualKeyboardWindowController* virtual_keyboard_window_controller_;
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardWindowControllerTest);
};

// Tests that the onscreen keyboard becomes enabled when maximize mode is
// enabled.
TEST_F(VirtualKeyboardWindowControllerTest, EnabledDuringMaximizeMode) {
  set_virtual_keyboard_window_controller(
      Shell::GetInstance()->display_controller()->
          virtual_keyboard_window_controller());

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
