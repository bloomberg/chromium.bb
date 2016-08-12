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
    views::PointerWatcher* watcher,
    bool wants_moves) {
  // We only allow a watcher to be added once. That is, we don't consider
  // the pair of |watcher| and |wants_move| unique, just |watcher|.
  if (wants_moves) {
    DCHECK(!non_move_watchers_.HasObserver(watcher));
    move_watchers_.AddObserver(watcher);
  } else {
    DCHECK(!move_watchers_.HasObserver(watcher));
    non_move_watchers_.AddObserver(watcher);
  }
}

void PointerWatcherDelegateAura::RemovePointerWatcher(
    views::PointerWatcher* watcher) {
  non_move_watchers_.RemoveObserver(watcher);
  move_watchers_.RemoveObserver(watcher);
}

void PointerWatcherDelegateAura::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED) {
    FOR_EACH_OBSERVER(views::PointerWatcher, non_move_watchers_,
                      OnMouseCaptureChanged());
    FOR_EACH_OBSERVER(views::PointerWatcher, move_watchers_,
                      OnMouseCaptureChanged());
    return;
  }

  // For compatibility with the mus version, don't send moves.
  if (event->type() != ui::ET_MOUSE_PRESSED &&
      event->type() != ui::ET_MOUSE_RELEASED)
    return;

  if (!ui::PointerEvent::CanConvertFrom(*event))
    return;

  NotifyWatchers(ui::PointerEvent(*event), *event);
}

void PointerWatcherDelegateAura::OnTouchEvent(ui::TouchEvent* event) {
  // For compatibility with the mus version, don't send moves.
  if (event->type() != ui::ET_TOUCH_PRESSED &&
      event->type() != ui::ET_TOUCH_RELEASED)
    return;

  DCHECK(ui::PointerEvent::CanConvertFrom(*event));
  NotifyWatchers(ui::PointerEvent(*event), *event);
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

void PointerWatcherDelegateAura::NotifyWatchers(
    const ui::PointerEvent& event,
    const ui::LocatedEvent& original_event) {
  const gfx::Point screen_location(GetLocationInScreen(original_event));
  views::Widget* target_widget = GetTargetWidget(original_event);
  FOR_EACH_OBSERVER(
      views::PointerWatcher, move_watchers_,
      OnPointerEventObserved(event, screen_location, target_widget));
  if (event.type() != ui::ET_MOUSE_MOVED &&
      event.type() != ui::ET_TOUCH_MOVED) {
    FOR_EACH_OBSERVER(
        views::PointerWatcher, non_move_watchers_,
        OnPointerEventObserved(event, screen_location, target_widget));
  }
}

}  // namespace ash
