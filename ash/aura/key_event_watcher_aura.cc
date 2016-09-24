// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/aura/key_event_watcher_aura.h"

#include "ash/shell.h"
#include "ui/events/event.h"

namespace ash {

KeyEventWatcherAura::KeyEventWatcherAura() {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

KeyEventWatcherAura::~KeyEventWatcherAura() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void KeyEventWatcherAura::OnKeyEvent(ui::KeyEvent* event) {
  if (HandleKeyEvent(*event))
    event->StopPropagation();
}

}  // namespace ash
