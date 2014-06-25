// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_CONTROL_DELEGATE_H_
#define ASH_IME_CONTROL_DELEGATE_H_

namespace ui {
class Accelerator;
}  // namespace ui

namespace ash {

// Delegate for controlling IME (input method editor).
class ImeControlDelegate {
 public:
  virtual ~ImeControlDelegate() {}

  // Changes the IME to what is listed next. This function do nothing if there
  // is only one IME is enabled.
  virtual void HandleNextIme() = 0;

  // Changes the IME to previously selected one. If there is no previously
  // selected IMEs, chooses the next listed IME. This function returns false if
  // there is only one IME is enabled.
  virtual bool HandlePreviousIme(const ui::Accelerator& accelerator) = 0;

  // Switches to another IME depending on the |accelerator|.
  virtual bool HandleSwitchIme(const ui::Accelerator& accelerator) = 0;

  // Checks for special language anomalies and re-map the |accelerator|
  // accordingly.
  virtual ui::Accelerator RemapAccelerator(
      const ui::Accelerator& accelerator) = 0;
};

}  // namespace ash

#endif  // ASH_IME_CONTROL_DELEGATE_H_
