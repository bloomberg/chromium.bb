// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/slow_animation_event_filter.h"

#include "ui/aura/event.h"
#include "ui/compositor/layer_animator.h"

namespace ash {
namespace internal {

SlowAnimationEventFilter::SlowAnimationEventFilter() {}

SlowAnimationEventFilter::~SlowAnimationEventFilter() {}

//////////////////////////////////////////////////////////////////////////////
// Overridden from aura::EventFilter:
bool SlowAnimationEventFilter::PreHandleKeyEvent(aura::Window* target,
                                                 aura::KeyEvent* event) {
  // We only want to trigger "slow animation" mode if the shift key is down,
  // no other modifier key is down, and the user isn't typing something like
  // Ctrl-Shift-W to close all windows.  Therefore we trigger the mode when
  // the last key press was exactly "shift key down" and otherwise clear it.
  ui::LayerAnimator::set_slow_animation_mode(IsUnmodifiedShiftPressed(event));
  // Let event processing continue.
  return false;
}

bool SlowAnimationEventFilter::PreHandleMouseEvent(aura::Window* target,
                                                   aura::MouseEvent* event) {
  return false;  // Not handled.
}

ui::TouchStatus SlowAnimationEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;  // Not handled.
}

ui::GestureStatus SlowAnimationEventFilter::PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;  // Not handled.
}

// Exists as a separate function for testing.
bool SlowAnimationEventFilter::IsUnmodifiedShiftPressed(
    aura::KeyEvent* event) const {
  return event->type() == ui::ET_KEY_PRESSED &&
      event->key_code() == ui::VKEY_SHIFT &&
      !event->IsControlDown() &&
      !event->IsAltDown();
}

}  // namespace internal
}  // namespace ash
