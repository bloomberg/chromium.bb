// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_VIRTUAL_KEYBOARD_WINDOW_CONTROLLER_H_
#define ASH_DISPLAY_VIRTUAL_KEYBOARD_WINDOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

namespace ash {

namespace internal {
class DisplayInfo;
class RootWindowController;

// This class maintains the RootWindowController dedicated for
// virtual keyboard.
class ASH_EXPORT VirtualKeyboardWindowController {
 public:
  VirtualKeyboardWindowController();
  virtual ~VirtualKeyboardWindowController();

  // Updates the root window's bounds using |display_info|.
  // Creates the new root window if one doesn't exist.
  void UpdateWindow(const DisplayInfo& display_info);

  // Close the mirror window.
  void Close();

 private:
  scoped_ptr<RootWindowController> root_window_controller_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardWindowController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_VIRTUAL_KEYBOARD_WINDOW_CONTROLLER_H_
