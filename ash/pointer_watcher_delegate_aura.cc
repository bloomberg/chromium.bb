// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/pointer_watcher_delegate_aura.h"

#include "ash/shell.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/pointer_watcher.h"

namespace ash {

PointerWatcherDelegateAura::PointerWatcherDelegateAura() {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

PointerWatcherDelegateAura::~PointerWatcherDelegateAura() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void PointerWatcherDelegateAura::AddPointerWatcher(
    views::PointerWatcher* watcher) {
  pointer_watchers_.AddObserver(watcher);
}

void PointerWatcherDelegateAura::RemovePointerWatcher(
    views::PointerWatcher* watcher) {
  pointer_watchers_.RemoveObserver(watcher);
}

void PointerWatcherDelegateAura::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    FOR_EACH_OBSERVER(views::PointerWatcher, pointer_watchers_,
                      OnMousePressed(*event));
}

void PointerWatcherDelegateAura::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED)
    FOR_EACH_OBSERVER(views::PointerWatcher, pointer_watchers_,
                      OnTouchPressed(*event));
}

}  // namespace ash
