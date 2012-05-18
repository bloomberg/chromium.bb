// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_gesture_event_filter.h"

#include "ash/shell.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"

namespace ash {
namespace internal {

SystemGestureEventFilter::SystemGestureEventFilter()
    : aura::EventFilter() {
}

SystemGestureEventFilter::~SystemGestureEventFilter() {
}

bool SystemGestureEventFilter::PreHandleKeyEvent(aura::Window* target,
                                                 aura::KeyEvent* event) {
  return false;
}

bool SystemGestureEventFilter::PreHandleMouseEvent(aura::Window* target,
                                                   aura::MouseEvent* event) {
  return false;
}

ui::TouchStatus SystemGestureEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus SystemGestureEventFilter::PreHandleGestureEvent(
    aura::Window* target, aura::GestureEvent* event) {
  // TODO(tdresser) handle system level gesture events
  if (event->type() == ui::ET_GESTURE_THREE_FINGER_SWIPE)
    return ui::GESTURE_STATUS_CONSUMED;
  if (target == Shell::GetRootWindow())
    return ui::GESTURE_STATUS_CONSUMED;
  return ui::GESTURE_STATUS_UNKNOWN;
}

}  // namespace internal
}  // namespace ash
