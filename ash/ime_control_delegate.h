// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_CONTROL_DELEGATE_H_
#define ASH_IME_CONTROL_DELEGATE_H_
#pragma once

namespace ui {
class Accelerator;
}  // namespace ui

namespace ash {

// Delegate for controlling IME (input method editor).
class ImeControlDelegate {
 public:
  virtual ~ImeControlDelegate() {}

  virtual bool HandleNextIme() = 0;
  virtual bool HandlePreviousIme() = 0;
  // Switches to another IME depending on the |accelerator|.
  virtual bool HandleSwitchIme(const ui::Accelerator& accelerator) = 0;

  // Checks for special language anomalies and re-map the |accelerator|
  // accordingly.
  virtual ui::Accelerator RemapAccelerator(
      const ui::Accelerator& accelerator) = 0;
};

}  // namespace ash

#endif  // ASH_IME_CONTROL_DELEGATE_H_
