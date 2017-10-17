// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_screenshot_controller_test_api.h"

#include "ash/system/power/power_button_screenshot_controller.h"

namespace ash {

PowerButtonScreenshotControllerTestApi::PowerButtonScreenshotControllerTestApi(
    PowerButtonScreenshotController* controller)
    : controller_(controller) {
  DCHECK(controller_);
}

PowerButtonScreenshotControllerTestApi::
    ~PowerButtonScreenshotControllerTestApi() = default;

bool PowerButtonScreenshotControllerTestApi::TriggerVolumeDownTimer() {
  if (!controller_->volume_down_timer_.IsRunning())
    return false;

  base::Closure task = controller_->volume_down_timer_.user_task();
  controller_->volume_down_timer_.Stop();
  task.Run();
  return true;
}

bool PowerButtonScreenshotControllerTestApi::
    ClamshellPowerButtonTimerIsRunning() const {
  return controller_->clamshell_power_button_timer_.IsRunning();
}

bool PowerButtonScreenshotControllerTestApi::
    TriggerClamshellPowerButtonTimer() {
  if (!controller_->clamshell_power_button_timer_.IsRunning())
    return false;

  base::Closure task = controller_->clamshell_power_button_timer_.user_task();
  controller_->clamshell_power_button_timer_.Stop();
  task.Run();
  return true;
}

}  // namespace ash
