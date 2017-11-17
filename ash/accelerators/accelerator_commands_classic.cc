// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands_classic.h"

#include "ash/shell.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_state.h"
#include "base/metrics/user_metrics.h"

namespace ash {
namespace accelerators {

void ToggleTouchHudProjection() {
  base::RecordAction(base::UserMetricsAction("Accel_Touch_Hud_Clear"));
  TouchDevicesController* controller = Shell::Get()->touch_devices_controller();
  controller->SetTouchHudProjectionEnabled(
      !controller->IsTouchHudProjectionEnabled());
}

void Unpin() {
  aura::Window* pinned_window =
      Shell::Get()->screen_pinning_controller()->pinned_window();
  if (pinned_window)
    wm::GetWindowState(pinned_window)->Restore();
}

}  // namespace accelerators
}  // namespace ash
