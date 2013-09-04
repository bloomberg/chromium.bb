// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_util.h"

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
  // Disable the shortcut for minimizing full screen window due to
  // crbug.com/131709, which is a crashing issue related to minimizing
  // full screen pepper window.
  if (wm::IsWindowFullscreen(window) || !wm::CanMinimizeWindow(window))
    return false;
  ash::Shell::GetInstance()->delegate()->RecordUserMetricsAction(
      ash::UMA_MINIMIZE_PER_KEY);
  wm::MinimizeWindow(window);
  return true;
}

}  // namespace accelerators
}  // namespace ash
