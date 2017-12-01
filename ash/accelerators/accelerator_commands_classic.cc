// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands_classic.h"

#include "ash/shell.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_state.h"

namespace ash {
namespace accelerators {

void Unpin() {
  aura::Window* pinned_window =
      Shell::Get()->screen_pinning_controller()->pinned_window();
  if (pinned_window)
    wm::GetWindowState(pinned_window)->Restore();
}

}  // namespace accelerators
}  // namespace ash
