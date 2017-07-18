// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/display_configuration_controller_test_api.h"

#include "ash/display/display_configuration_controller.h"

namespace ash {

DisplayConfigurationControllerTestApi::DisplayConfigurationControllerTestApi(
    DisplayConfigurationController* controller)
    : controller_(controller) {}

void DisplayConfigurationControllerTestApi::DisableDisplayAnimator() {
  controller_->ResetAnimatorForTest();
}

ScreenRotationAnimator*
DisplayConfigurationControllerTestApi::GetScreenRotationAnimatorForDisplay(
    int64_t display_id) {
  return controller_->GetScreenRotationAnimatorForDisplay(display_id);
}

}  // namespace ash
