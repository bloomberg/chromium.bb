// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/tablet_power_button_controller_test_api.h"

#include "ash/system/power/power_button_display_controller.h"
#include "ash/system/power/tablet_power_button_controller.h"

namespace ash {

TabletPowerButtonControllerTestApi::TabletPowerButtonControllerTestApi(
    TabletPowerButtonController* controller)
    : controller_(controller) {}

TabletPowerButtonControllerTestApi::~TabletPowerButtonControllerTestApi() =
    default;

bool TabletPowerButtonControllerTestApi::ShutdownTimerIsRunning() const {
  return controller_->shutdown_timer_.IsRunning();
}

bool TabletPowerButtonControllerTestApi::TriggerShutdownTimeout() {
  if (!controller_->shutdown_timer_.IsRunning())
    return false;

  base::Closure task = controller_->shutdown_timer_.user_task();
  controller_->shutdown_timer_.Stop();
  task.Run();
  return true;
}

void TabletPowerButtonControllerTestApi::SendKeyEvent(ui::KeyEvent* event) {
  controller_->display_controller_->OnKeyEvent(event);
}

}  // namespace ash
