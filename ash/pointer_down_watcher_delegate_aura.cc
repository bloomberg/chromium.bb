// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/pointer_down_watcher_delegate_aura.h"

#include "ash/shell.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/pointer_down_watcher.h"
#include "ui/views/widget/widget.h"

namespace ash {

PointerDownWatcherDelegateAura::PointerDownWatcherDelegateAura() {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

PointerDownWatcherDelegateAura::~PointerDownWatcherDelegateAura() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void PointerDownWatcherDelegateAura::AddPointerDownWatcher(
    views::PointerDownWatcher* watcher) {
  pointer_down_watchers_.AddObserver(watcher);
}

void PointerDownWatcherDelegateAura::RemovePointerDownWatcher(
    views::PointerDownWatcher* watcher) {
  pointer_down_watchers_.RemoveObserver(watcher);
}

void PointerDownWatcherDelegateAura::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    FOR_EACH_OBSERVER(views::PointerDownWatcher, pointer_down_watchers_,
                      OnMousePressed(*event, GetLocationInScreen(*event),
                                     GetTargetWidget(*event)));
}

void PointerDownWatcherDelegateAura::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED)
    FOR_EACH_OBSERVER(views::PointerDownWatcher, pointer_down_watchers_,
                      OnTouchPressed(*event, GetLocationInScreen(*event),
                                     GetTargetWidget(*event)));
}

gfx::Point PointerDownWatcherDelegateAura::GetLocationInScreen(
    const ui::LocatedEvent& event) const {
  aura::Window* target = static_cast<aura::Window*>(event.target());
  gfx::Point location_in_screen = event.location();
  aura::client::GetScreenPositionClient(target->GetRootWindow())
      ->ConvertPointToScreen(target, &location_in_screen);
  return location_in_screen;
}

views::Widget* PointerDownWatcherDelegateAura::GetTargetWidget(
    const ui::LocatedEvent& event) const {
  aura::Window* window = static_cast<aura::Window*>(event.target());
  return views::Widget::GetTopLevelWidgetForNativeView(window);
}

}  // namespace ash
