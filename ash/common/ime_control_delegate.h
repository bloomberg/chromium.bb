// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_IME_CONTROL_DELEGATE_H_
#define ASH_COMMON_IME_CONTROL_DELEGATE_H_

namespace ui {
class Accelerator;
}  // namespace ui

namespace ash {

// Delegate for controlling IME (input method editor).
class ImeControlDelegate {
 public:
  virtual ~ImeControlDelegate() {}

  // Returns whether a user can cycle through IMEs. Returns false if only one
  // IME is enabled.
  virtual bool CanCycleIme() = 0;

  // Changes the IME to what is listed next.
  virtual void HandleNextIme() = 0;

  // Changes the IME to previously selected one. If there is no previously
  // selected IMEs, chooses the next listed IME.
  virtual void HandlePreviousIme() = 0;

  // Returns true if the IME can be switched to the IME associated with
  // |accelerator|.
  virtual bool CanSwitchIme(const ui::Accelerator& accelerator) = 0;

  // Switches to the IME associated with |accelerator|.
  virtual void HandleSwitchIme(const ui::Accelerator& accelerator) = 0;
};

}  // namespace ash

#endif  // ASH_COMMON_IME_CONTROL_DELEGATE_H_
