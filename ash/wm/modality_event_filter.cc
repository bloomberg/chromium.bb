// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/modality_event_filter.h"

#include "ash/wm/modality_event_filter_delegate.h"
#include "ui/aura/event.h"

namespace ash {
namespace internal {

ModalityEventFilter::ModalityEventFilter(aura::Window* container,
                                         ModalityEventFilterDelegate* delegate)
    : EventFilter(container),
      delegate_(delegate) {
}

ModalityEventFilter::~ModalityEventFilter() {
}

bool ModalityEventFilter::PreHandleKeyEvent(aura::Window* target,
                                            aura::KeyEvent* event) {
  return !delegate_->CanWindowReceiveEvents(target);
}

bool ModalityEventFilter::PreHandleMouseEvent(aura::Window* target,
                                              aura::MouseEvent* event) {
  return !delegate_->CanWindowReceiveEvents(target);
}

ui::TouchStatus ModalityEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  // TODO(sadrul): !
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus ModalityEventFilter::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  // TODO(sad):
  return ui::GESTURE_STATUS_UNKNOWN;
}

}  // namespace internal
}  // namespace ash
