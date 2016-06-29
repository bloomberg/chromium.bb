// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_event_handler_aura.h"

#include "ash/shell.h"
#include "ui/events/event.h"

namespace ash {
namespace wm {

MaximizeModeEventHandlerAura::MaximizeModeEventHandlerAura() {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

MaximizeModeEventHandlerAura::~MaximizeModeEventHandlerAura() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void MaximizeModeEventHandlerAura::OnTouchEvent(ui::TouchEvent* event) {
  if (ToggleFullscreen(*event))
    event->StopPropagation();
}

}  // namespace wm
}  // namespace ash
