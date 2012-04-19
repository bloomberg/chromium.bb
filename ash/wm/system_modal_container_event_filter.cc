// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_modal_container_event_filter.h"

#include "ash/wm/system_modal_container_event_filter_delegate.h"
#include "ui/aura/event.h"

namespace ash {
namespace internal {

SystemModalContainerEventFilter::SystemModalContainerEventFilter(
    SystemModalContainerEventFilterDelegate* delegate)
    : delegate_(delegate) {
}

SystemModalContainerEventFilter::~SystemModalContainerEventFilter() {
}

bool SystemModalContainerEventFilter::PreHandleKeyEvent(
    aura::Window* target,
    aura::KeyEvent* event) {
  return !delegate_->CanWindowReceiveEvents(target);
}

bool SystemModalContainerEventFilter::PreHandleMouseEvent(
    aura::Window* target,
    aura::MouseEvent* event) {
  return !delegate_->CanWindowReceiveEvents(target);
}

ui::TouchStatus SystemModalContainerEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  // TODO(sadrul): !
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus SystemModalContainerEventFilter::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  // TODO(sad):
  return ui::GESTURE_STATUS_UNKNOWN;
}

}  // namespace internal
}  // namespace ash
