// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/pointer_watcher_delegate_aura.h"

#include "ash/shell.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/pointer_watcher.h"
#include "ui/views/widget/widget.h"

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
                      OnMousePressed(*event, GetLocationInScreen(*event),
                                     GetTargetWidget(*event)));
}

void PointerWatcherDelegateAura::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED)
    FOR_EACH_OBSERVER(views::PointerWatcher, pointer_watchers_,
                      OnTouchPressed(*event, GetLocationInScreen(*event),
                                     GetTargetWidget(*event)));
}

gfx::Point PointerWatcherDelegateAura::GetLocationInScreen(
    const ui::LocatedEvent& event) const {
  aura::Window* target = static_cast<aura::Window*>(event.target());
  gfx::Point location_in_screen = event.location();
  aura::client::GetScreenPositionClient(target->GetRootWindow())
      ->ConvertPointToScreen(target, &location_in_screen);
  return location_in_screen;
}

views::Widget* PointerWatcherDelegateAura::GetTargetWidget(
    const ui::LocatedEvent& event) const {
  aura::Window* window = static_cast<aura::Window*>(event.target());
  return views::Widget::GetTopLevelWidgetForNativeView(window);
}

}  // namespace ash
