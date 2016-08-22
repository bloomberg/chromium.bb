// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands_aura.h"

#include "ash/common/display/display_info.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_window.h"
#include "ash/display/display_manager.h"
#include "ash/display/display_util.h"
#include "ash/shell.h"
#include "ash/wm/screen_pinning_controller.h"
#include "base/metrics/user_metrics.h"

namespace ash {
namespace accelerators {

void ToggleTouchHudProjection() {
  base::RecordAction(base::UserMetricsAction("Accel_Touch_Hud_Clear"));
  bool enabled = Shell::GetInstance()->is_touch_hud_projection_enabled();
  Shell::GetInstance()->SetTouchHudProjectionEnabled(!enabled);
}

bool IsInternalDisplayZoomEnabled() {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  return display_manager->IsDisplayUIScalingEnabled() ||
         display_manager->IsInUnifiedMode();
}

bool ZoomInternalDisplay(bool up) {
  if (up)
    base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Up"));
  else
    base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Down"));

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();

  int64_t display_id = display_manager->IsInUnifiedMode()
                           ? DisplayManager::kUnifiedDisplayId
                           : display_manager->GetDisplayIdForUIScaling();
  const DisplayInfo& display_info = display_manager->GetDisplayInfo(display_id);

  scoped_refptr<ManagedDisplayMode> mode;
  if (display_manager->IsInUnifiedMode())
    mode = GetDisplayModeForNextResolution(display_info, up);
  else
    mode = GetDisplayModeForNextUIScale(display_info, up);

  if (!mode)
    return false;
  return display_manager->SetDisplayMode(display_id, mode);
}

void ResetInternalDisplayZoom() {
  base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Reset"));
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();

  if (display_manager->IsInUnifiedMode()) {
    const DisplayInfo& display_info =
        display_manager->GetDisplayInfo(DisplayManager::kUnifiedDisplayId);
    const DisplayInfo::ManagedDisplayModeList& modes =
        display_info.display_modes();
    auto iter = std::find_if(modes.begin(), modes.end(),
                             [](const scoped_refptr<ManagedDisplayMode>& mode) {
                               return mode->native();
                             });
    display_manager->SetDisplayMode(DisplayManager::kUnifiedDisplayId, *iter);
  } else {
    SetDisplayUIScale(display_manager->GetDisplayIdForUIScaling(), 1.0f);
  }
}

void Unpin() {
  WmWindow* pinned_window =
      Shell::GetInstance()->screen_pinning_controller()->pinned_window();
  if (pinned_window)
    pinned_window->GetWindowState()->Restore();
}

}  // namespace accelerators
}  // namespace ash
