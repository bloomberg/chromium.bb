// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_BRIGHTNESS_CONTROLLER_CHROMEOS_H_
#define ASH_SYSTEM_BRIGHTNESS_BRIGHTNESS_CONTROLLER_CHROMEOS_H_

#include "ash/ash_export.h"
#include "ash/system/brightness_control_delegate.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/optional.h"

namespace ash {
namespace system {

// A class which controls brightness when F6, F7 or a multimedia key for
// brightness is pressed.
class ASH_EXPORT BrightnessControllerChromeos
    : public ash::BrightnessControlDelegate {
 public:
  BrightnessControllerChromeos() {}
  ~BrightnessControllerChromeos() override {}

  // Overridden from ash::BrightnessControlDelegate:
  void HandleBrightnessDown(const ui::Accelerator& accelerator) override;
  void HandleBrightnessUp(const ui::Accelerator& accelerator) override;
  void SetBrightnessPercent(double percent, bool gradual) override;
  void GetBrightnessPercent(
      base::OnceCallback<void(base::Optional<double>)> callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrightnessControllerChromeos);
};

}  // namespace system
}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_BRIGHTNESS_CONTROLLER_CHROMEOS_H_
