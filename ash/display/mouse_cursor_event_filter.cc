// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mouse_cursor_event_filter.h"

#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "ash/wm/cursor_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/event.h"

namespace ash {
namespace internal {

MouseCursorEventFilter::MouseCursorEventFilter(
    DisplayController* display_controller)
    : display_controller_(display_controller) {
  DCHECK(display_controller_);
}

MouseCursorEventFilter::~MouseCursorEventFilter() {
}

bool MouseCursorEventFilter::PreHandleKeyEvent(aura::Window* target,
                                               ui::KeyEvent* event) {
  return false;
}

bool MouseCursorEventFilter::PreHandleMouseEvent(aura::Window* target,
                                                 ui::MouseEvent* event) {
  // Handle both MOVED and DRAGGED events here because when the mouse pointer
  // enters the other root window while dragging, the underlying window system
  // (at least X11) stops generating a ui::ET_MOUSE_MOVED event.
  if (event->type() != ui::ET_MOUSE_MOVED &&
      event->type() != ui::ET_MOUSE_DRAGGED) {
      return false;
  }

  aura::RootWindow* current_root = target->GetRootWindow();
  gfx::Point location_in_root(event->location());
  aura::Window::ConvertPointToTarget(target, current_root, &location_in_root);
  return display_controller_->WarpMouseCursorIfNecessary(
      current_root, location_in_root);
}

ui::TouchStatus MouseCursorEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    ui::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus MouseCursorEventFilter::PreHandleGestureEvent(
    aura::Window* target,
    ui::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;
}

}  // namespace internal
}  // namespace ash
