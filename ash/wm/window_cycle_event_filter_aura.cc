// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_event_filter_aura.h"

#include "ash/shell.h"
#include "ash/wm/window_cycle_controller.h"
#include "ui/events/event.h"

namespace ash {

WindowCycleEventFilterAura::WindowCycleEventFilterAura() {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

WindowCycleEventFilterAura::~WindowCycleEventFilterAura() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void WindowCycleEventFilterAura::OnKeyEvent(ui::KeyEvent* event) {
  // Until the alt key is released, all key events are handled by this window
  // cycle controller: https://crbug.com/340339.
  event->StopPropagation();
  // Views uses VKEY_MENU for both left and right Alt keys.
  if (event->key_code() == ui::VKEY_MENU &&
      event->type() == ui::ET_KEY_RELEASED) {
    Shell::GetInstance()->window_cycle_controller()->StopCycling();
    // Warning: |this| will be deleted from here on.
  }
}

}  // namespace ash
