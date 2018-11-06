// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_gesture_event_filter.h"

#include "ash/touch/touch_uma.h"
#include "ash/wm/gestures/overview_gesture_handler.h"
#include "base/metrics/user_metrics.h"
#include "ui/aura/window.h"
#include "ui/base/pointer/pointer_device.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace ash {

SystemGestureEventFilter::SystemGestureEventFilter()
    : overview_gesture_handler_(new OverviewGestureHandler) {}

SystemGestureEventFilter::~SystemGestureEventFilter() = default;

void SystemGestureEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED &&
      ui::GetTouchScreensAvailability() ==
          ui::TouchScreensAvailability::ENABLED) {
    base::RecordAction(base::UserMetricsAction("Mouse_Down"));
  }
}

void SystemGestureEventFilter::OnScrollEvent(ui::ScrollEvent* event) {
  if (overview_gesture_handler_ &&
      overview_gesture_handler_->ProcessScrollEvent(*event)) {
    event->StopPropagation();
  }
}

void SystemGestureEventFilter::OnTouchEvent(ui::TouchEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  TouchUMA::GetInstance()->RecordTouchEvent(target, *event);
}

void SystemGestureEventFilter::OnGestureEvent(ui::GestureEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  TouchUMA::GetInstance()->RecordGestureEvent(target, *event);
}

}  // namespace ash
