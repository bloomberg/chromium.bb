// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands_aura.h"

#include "ash/common/wm/window_state.h"
#include "ash/common/wm_window.h"
#include "ash/shell.h"
#include "ash/wm/screen_pinning_controller.h"
#include "base/metrics/user_metrics.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"

namespace ash {
namespace accelerators {

void ToggleTouchHudProjection() {
  base::RecordAction(base::UserMetricsAction("Accel_Touch_Hud_Clear"));
  bool enabled = Shell::GetInstance()->is_touch_hud_projection_enabled();
  Shell::GetInstance()->SetTouchHudProjectionEnabled(!enabled);
}

bool IsInternalDisplayZoomEnabled() {
  display::DisplayManager* display_manager =
      Shell::GetInstance()->display_manager();
  return display_manager->IsDisplayUIScalingEnabled() ||
         display_manager->IsInUnifiedMode();
}

bool ZoomInternalDisplay(bool up) {
  if (up)
    base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Up"));
  else
    base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Down"));

  display::DisplayManager* display_manager =
      Shell::GetInstance()->display_manager();
  return display_manager->ZoomInternalDisplay(up);
}

void ResetInternalDisplayZoom() {
  base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Reset"));
  display::DisplayManager* display_manager =
      Shell::GetInstance()->display_manager();
  display_manager->ResetInternalDisplayZoom();
}

void Unpin() {
  WmWindow* pinned_window =
      Shell::GetInstance()->screen_pinning_controller()->pinned_window();
  if (pinned_window)
    pinned_window->GetWindowState()->Restore();
}

}  // namespace accelerators
}  // namespace ash
