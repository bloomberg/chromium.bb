// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/accelerators/accelerator_commands.h"

#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/wm_event.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/metrics/user_metrics.h"

namespace ash {
namespace accelerators {

bool ToggleMinimized() {
  WmWindow* window = WmShell::Get()->GetActiveWindow();
  // Attempt to restore the window that would be cycled through next from
  // the launcher when there is no active window.
  if (!window) {
    MruWindowTracker::WindowList mru_windows(
        WmShell::Get()->mru_window_tracker()->BuildMruWindowList());
    if (!mru_windows.empty())
      mru_windows.front()->GetWindowState()->Activate();
    return true;
  }
  wm::WindowState* window_state = window->GetWindowState();
  if (!window_state->CanMinimize())
    return false;
  window_state->Minimize();
  return true;
}

void ToggleMaximized() {
  WmWindow* active_window = WmShell::Get()->GetActiveWindow();
  if (!active_window)
    return;
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Maximized"));
  wm::WMEvent event(wm::WM_EVENT_TOGGLE_MAXIMIZE);
  active_window->GetWindowState()->OnWMEvent(&event);
}

void ToggleFullscreen() {
  WmWindow* active_window = WmShell::Get()->GetActiveWindow();
  if (!active_window)
    return;
  const wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  active_window->GetWindowState()->OnWMEvent(&event);
}

}  // namespace accelerators
}  // namespace ash
