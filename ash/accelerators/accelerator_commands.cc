// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/metrics/user_metrics.h"

namespace ash {
namespace accelerators {

bool ToggleMinimized() {
  aura::Window* window = wm::GetActiveWindow();
  // Attempt to restore the window that would be cycled through next from
  // the launcher when there is no active window.
  if (!window) {
    ash::Shell::GetInstance()->window_cycle_controller()->
        HandleCycleWindow(WindowCycleController::FORWARD, false);
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
  window_state->OnWMEvent(wm::TOGGLE_MAXIMIZE);
}

void ToggleFullscreen() {
  wm::WindowState* window_state = wm::GetActiveWindowState();
  if (window_state)
    window_state->ToggleFullscreen();
}

}  // namespace accelerators
}  // namespace ash
