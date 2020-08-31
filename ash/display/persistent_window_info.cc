// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/persistent_window_info.h"

#include "ash/wm/window_state.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

PersistentWindowInfo::PersistentWindowInfo(aura::Window* window) {
  const auto& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  window_bounds_in_screen = window->GetBoundsInScreen();
  display_id = display.id();
  display_bounds_in_screen = display.bounds();

  WindowState* window_state = WindowState::Get(window);
  DCHECK(window_state);
  if (window_state->HasRestoreBounds())
    restore_bounds_in_screen = window_state->GetRestoreBoundsInScreen();
}

PersistentWindowInfo::PersistentWindowInfo(const PersistentWindowInfo& other) =
    default;

PersistentWindowInfo::~PersistentWindowInfo() = default;

}  // namespace ash
