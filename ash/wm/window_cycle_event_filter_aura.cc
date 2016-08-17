// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_event_filter_aura.h"

#include "ash/common/wm/window_cycle_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "ui/events/event.h"

namespace ash {

WindowCycleEventFilterAura::WindowCycleEventFilterAura() {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

WindowCycleEventFilterAura::~WindowCycleEventFilterAura() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void WindowCycleEventFilterAura::OnKeyEvent(ui::KeyEvent* event) {
  // Until the alt key is released, all key events except the tab press (which
  // is handled by the accelerator controller to call Step) are handled by this
  // window cycle controller: https://crbug.com/340339.
  if (event->key_code() != ui::VKEY_TAB ||
      event->type() != ui::ET_KEY_PRESSED) {
    event->StopPropagation();
  }
  // Views uses VKEY_MENU for both left and right Alt keys.
  if (event->key_code() == ui::VKEY_MENU &&
      event->type() == ui::ET_KEY_RELEASED) {
    WmShell::Get()->window_cycle_controller()->StopCycling();
    // Warning: |this| will be deleted from here on.
  } else if (event->key_code() == ui::VKEY_TAB) {
    if (event->type() == ui::ET_KEY_RELEASED) {
      repeat_timer_.Stop();
    } else if (event->type() == ui::ET_KEY_PRESSED && event->is_repeat() &&
               !repeat_timer_.IsRunning()) {
      repeat_timer_.Start(
          FROM_HERE, base::TimeDelta::FromMilliseconds(180),
          base::Bind(
              &WindowCycleController::HandleCycleWindow,
              base::Unretained(WmShell::Get()->window_cycle_controller()),
              event->IsShiftDown() ? WindowCycleController::BACKWARD
                                   : WindowCycleController::FORWARD));
    }
  }
}

}  // namespace ash
