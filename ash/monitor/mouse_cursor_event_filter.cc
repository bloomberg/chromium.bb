// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/monitor/mouse_cursor_event_filter.h"

#include "ash/monitor/monitor_controller.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {

MouseCursorEventFilter::MouseCursorEventFilter(
    MonitorController* monitor_controller)
    : monitor_controller_(monitor_controller) {
  DCHECK(monitor_controller_);
}

MouseCursorEventFilter::~MouseCursorEventFilter() {
}

bool MouseCursorEventFilter::PreHandleKeyEvent(aura::Window* target,
                                               aura::KeyEvent* event) {
  return false;
}

bool MouseCursorEventFilter::PreHandleMouseEvent(aura::Window* target,
                                                 aura::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_MOVED)
    return false;
  aura::RootWindow* current_root = target->GetRootWindow();
  gfx::Point location_in_root(event->location());
  aura::Window::ConvertPointToWindow(target, current_root, &location_in_root);
  return monitor_controller_->WarpMouseCursorIfNecessary(
      current_root, location_in_root);
}

ui::TouchStatus MouseCursorEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus MouseCursorEventFilter::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;
}

}  // namespace internal
}  // namespace ash
