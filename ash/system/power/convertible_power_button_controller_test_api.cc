// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/convertible_power_button_controller_test_api.h"

#include "ash/system/power/convertible_power_button_controller.h"
#include "ash/system/power/power_button_display_controller.h"

namespace ash {

ConvertiblePowerButtonControllerTestApi::
    ConvertiblePowerButtonControllerTestApi(
        ConvertiblePowerButtonController* controller)
    : controller_(controller) {}

ConvertiblePowerButtonControllerTestApi::
    ~ConvertiblePowerButtonControllerTestApi() = default;

bool ConvertiblePowerButtonControllerTestApi::ShutdownTimerIsRunning() const {
  return controller_->shutdown_timer_.IsRunning();
}

bool ConvertiblePowerButtonControllerTestApi::TriggerShutdownTimeout() {
  if (!controller_->shutdown_timer_.IsRunning())
    return false;

  base::Closure task = controller_->shutdown_timer_.user_task();
  controller_->shutdown_timer_.Stop();
  task.Run();
  return true;
}

void ConvertiblePowerButtonControllerTestApi::SendKeyEvent(
    ui::KeyEvent* event) {
  controller_->display_controller_->OnKeyEvent(event);
}

}  // namespace ash
