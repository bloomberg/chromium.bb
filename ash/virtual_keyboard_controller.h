// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_VIRTUAL_KEYBOARD_CONTROLLER_H_
#define ASH_VIRTUAL_KEYBOARD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"

namespace ash {

// This class observes enter/leaving maximized mode for virtual keyboard.
class ASH_EXPORT VirtualKeyboardController : public ShellObserver {
 public:
  VirtualKeyboardController();
  virtual ~VirtualKeyboardController();

  // ShellObserver:
  virtual void OnMaximizeModeStarted() OVERRIDE;
  virtual void OnMaximizeModeEnded() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardController);
};

}  // namespace ash

#endif  // ASH_VIRTUAL_KEYBOARD_CONTROLLER_H_
