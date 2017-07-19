// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_event_handler_aura.h"

#include "ash/shell.h"
#include "ui/events/event.h"

namespace ash {
namespace wm {

TabletModeEventHandlerAura::TabletModeEventHandlerAura() {
  Shell::Get()->AddPreTargetHandler(this);
}

TabletModeEventHandlerAura::~TabletModeEventHandlerAura() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void TabletModeEventHandlerAura::OnTouchEvent(ui::TouchEvent* event) {
  if (ToggleFullscreen(*event))
    event->StopPropagation();
}

}  // namespace wm
}  // namespace ash
