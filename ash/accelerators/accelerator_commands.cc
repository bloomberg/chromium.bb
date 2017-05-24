// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/metrics/user_metrics.h"

namespace ash {
namespace accelerators {

bool ToggleMinimized() {
  aura::Window* window = wm::GetActiveWindow();
  // Attempt to restore the window that would be cycled through next from
  // the launcher when there is no active window.
  if (!window) {
    MruWindowTracker::WindowList mru_windows(
        Shell::Get()->mru_window_tracker()->BuildMruWindowList());
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
  aura::Window* active_window = wm::GetActiveWindow();
  if (!active_window)
    return;
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Maximized"));
  wm::WMEvent event(wm::WM_EVENT_TOGGLE_MAXIMIZE);
  wm::GetWindowState(active_window)->OnWMEvent(&event);
}

void ToggleFullscreen() {
  aura::Window* active_window = wm::GetActiveWindow();
  if (!active_window)
    return;
  const wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  wm::GetWindowState(active_window)->OnWMEvent(&event);
}

}  // namespace accelerators
}  // namespace ash
