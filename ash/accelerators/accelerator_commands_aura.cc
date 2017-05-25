// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands_aura.h"

#include "ash/shell.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_state.h"
#include "base/metrics/user_metrics.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"

namespace ash {
namespace accelerators {

void ToggleTouchHudProjection() {
  base::RecordAction(base::UserMetricsAction("Accel_Touch_Hud_Clear"));
  bool enabled = Shell::Get()->is_touch_hud_projection_enabled();
  Shell::Get()->SetTouchHudProjectionEnabled(!enabled);
}

bool IsInternalDisplayZoomEnabled() {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  return display_manager->IsDisplayUIScalingEnabled() ||
         display_manager->IsInUnifiedMode();
}

bool ZoomInternalDisplay(bool up) {
  if (up)
    base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Up"));
  else
    base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Down"));

  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  return display_manager->ZoomInternalDisplay(up);
}

void ResetInternalDisplayZoom() {
  base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Reset"));
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display_manager->ResetInternalDisplayZoom();
}

void Unpin() {
  aura::Window* pinned_window =
      Shell::Get()->screen_pinning_controller()->pinned_window();
  if (pinned_window)
    wm::GetWindowState(pinned_window)->Restore();
}

}  // namespace accelerators
}  // namespace ash
