// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_BRIGHTNESS_BRIGHTNESS_CONTROLLER_CHROMEOS_H_
#define ASH_SYSTEM_CHROMEOS_BRIGHTNESS_BRIGHTNESS_CONTROLLER_CHROMEOS_H_

#include "ash/ash_export.h"
#include "ash/system/brightness_control_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace ash {
namespace system {

// A class which controls brightness when F6, F7 or a multimedia key for
// brightness is pressed.
class ASH_EXPORT BrightnessControllerChromeos
    : public ash::BrightnessControlDelegate {
 public:
  BrightnessControllerChromeos() {}
  virtual ~BrightnessControllerChromeos() {}

  // Overridden from ash::BrightnessControlDelegate:
  virtual bool HandleBrightnessDown(
      const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool HandleBrightnessUp(const ui::Accelerator& accelerator) OVERRIDE;
  virtual void SetBrightnessPercent(double percent, bool gradual) OVERRIDE;
  virtual void GetBrightnessPercent(
      const base::Callback<void(double)>& callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrightnessControllerChromeos);
};

}  // namespace system
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_BRIGHTNESS_BRIGHTNESS_CONTROLLER_CHROMEOS_H_
