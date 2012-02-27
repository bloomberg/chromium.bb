// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_BRIGHTNESS_CONTROL_DELEGATE_H_
#define ASH_SYSTEM_BRIGHTNESS_BRIGHTNESS_CONTROL_DELEGATE_H_
#pragma once

namespace ui {
class Accelerator;
}  // namespace ui

namespace ash {

// Delegate for controlling the brightness.
class BrightnessControlDelegate {
 public:
  virtual ~BrightnessControlDelegate() {}

  virtual bool HandleBrightnessDown(const ui::Accelerator& accelerator) = 0;
  virtual bool HandleBrightnessUp(const ui::Accelerator& accelerator) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_BRIGHTNESS_CONTROL_DELEGATE_H_
