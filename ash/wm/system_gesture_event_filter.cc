// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_gesture_event_filter.h"

#include "ash/ash_switches.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "ash/touch/touch_uma.h"
#include "ash/wm/gestures/overview_gesture_handler.h"
#include "ash/wm/gestures/shelf_gesture_handler.h"
#include "ui/base/touch/touch_device.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace ash {

SystemGestureEventFilter::SystemGestureEventFilter()
    : overview_gesture_handler_(new OverviewGestureHandler),
      shelf_gesture_handler_(new ShelfGestureHandler()) {}

SystemGestureEventFilter::~SystemGestureEventFilter() {
}

void SystemGestureEventFilter::OnMouseEvent(ui::MouseEvent* event) {
#if defined(OS_CHROMEOS)
  if (event->type() == ui::ET_MOUSE_PRESSED &&
      ui::GetTouchScreensAvailability() ==
          ui::TouchScreensAvailability::ENABLED) {
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(UMA_MOUSE_DOWN);
  }
#endif
}

void SystemGestureEventFilter::OnScrollEvent(ui::ScrollEvent* event) {
  if (overview_gesture_handler_ &&
      overview_gesture_handler_->ProcessScrollEvent(*event)) {
    event->StopPropagation();
  }
}

void SystemGestureEventFilter::OnTouchEvent(ui::TouchEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  ash::TouchUMA::GetInstance()->RecordTouchEvent(target, *event);
}

void SystemGestureEventFilter::OnGestureEvent(ui::GestureEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  ash::TouchUMA::GetInstance()->RecordGestureEvent(target, *event);

  if (event->type() == ui::ET_GESTURE_WIN8_EDGE_SWIPE &&
      shelf_gesture_handler_->ProcessGestureEvent(*event, target)) {
    // Do not stop propagation, since the immersive fullscreen controller may
    // need to handle this event.
    return;
  }
}

}  // namespace ash
