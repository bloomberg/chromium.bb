// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include "ash/display/display_manager.h"
#include "ash/display/display_util.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/common/wm_event.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "base/metrics/user_metrics.h"

namespace ash {
namespace accelerators {

bool ToggleMinimized() {
  aura::Window* window = wm::GetActiveWindow();
  // Attempt to restore the window that would be cycled through next from
  // the launcher when there is no active window.
  if (!window) {
    MruWindowTracker::WindowList mru_windows(
        Shell::GetInstance()->mru_window_tracker()->BuildMruWindowList());
    if (!mru_windows.empty())
      wm::GetWindowState(mru_windows.front())->Activate();
    return true;
  }
  wm::WindowState* window_state = wm::GetWindowState(window);
  if (!window_state->CanMinimize())
    return false;
  window_state->Minimize();
  return true;
}

void ToggleMaximized() {
  wm::WindowState* window_state = wm::GetActiveWindowState();
  if (!window_state)
    return;
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Maximized"));
  wm::WMEvent event(wm::WM_EVENT_TOGGLE_MAXIMIZE);
  window_state->OnWMEvent(&event);
}

void ToggleFullscreen() {
  wm::WindowState* window_state = wm::GetActiveWindowState();
  if (window_state) {
    const wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
    window_state->OnWMEvent(&event);
  }
}

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
  DisplayMode mode;

  if (display_manager->IsInUnifiedMode()) {
    if (!GetDisplayModeForNextResolution(display_info, up, &mode))
      return false;
  } else {
    if (!GetDisplayModeForNextUIScale(display_info, up, &mode))
      return false;
  }
  return display_manager->SetDisplayMode(display_id, mode);
}

void ResetInternalDisplayZoom() {
  base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Reset"));
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();

  if (display_manager->IsInUnifiedMode()) {
    const DisplayInfo& display_info =
        display_manager->GetDisplayInfo(DisplayManager::kUnifiedDisplayId);
    const std::vector<DisplayMode>& modes = display_info.display_modes();
    auto iter =
        std::find_if(modes.begin(), modes.end(),
                     [](const DisplayMode& mode) { return mode.native; });
    display_manager->SetDisplayMode(DisplayManager::kUnifiedDisplayId, *iter);
  } else {
    SetDisplayUIScale(display_manager->GetDisplayIdForUIScaling(), 1.0f);
  }
}

}  // namespace accelerators
}  // namespace ash
