// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_VIRTUAL_KEYBOARD_WINDOW_CONTROLLER_H_
#define ASH_DISPLAY_VIRTUAL_KEYBOARD_WINDOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/display.h"

namespace keyboard {
class KeyboardController;
}

namespace ash {
class DisplayInfo;
class RootWindowController;

namespace test {
class VirtualKeyboardWindowControllerTest;
}  // namespace test

// This class maintains the RootWindowController dedicated for
// virtual keyboard.
class ASH_EXPORT VirtualKeyboardWindowController : public ShellObserver {
 public:
  VirtualKeyboardWindowController();
  virtual ~VirtualKeyboardWindowController();

  void ActivateKeyboard(keyboard::KeyboardController* keyboard_controller);

  // Updates the root window's bounds using |display_info|.
  // Creates the new root window if one doesn't exist.
  void UpdateWindow(const DisplayInfo& display_info);

  // Close the mirror window.
  void Close();

  // ShellObserver:
  virtual void OnMaximizeModeStarted() OVERRIDE;
  virtual void OnMaximizeModeEnded() OVERRIDE;

 private:
  friend class test::VirtualKeyboardWindowControllerTest;

  // Rotates virtual keyboard display by 180 degrees.
  void FlipDisplay();

  RootWindowController* root_window_controller_for_test() {
    return root_window_controller_.get();
  }

  scoped_ptr<RootWindowController> root_window_controller_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardWindowController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_VIRTUAL_KEYBOARD_WINDOW_CONTROLLER_H_
