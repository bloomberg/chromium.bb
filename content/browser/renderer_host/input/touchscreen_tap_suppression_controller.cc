// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touchscreen_tap_suppression_controller.h"

#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/tap_suppression_controller.h"
#include "ui/events/gestures/gesture_configuration.h"

#if defined(OS_ANDROID)
#include "ui/gfx/android/view_configuration.h"
#endif

namespace content {

TouchscreenTapSuppressionController::TouchscreenTapSuppressionController(
    GestureEventQueue* geq)
    : gesture_event_queue_(geq),
      controller_(new TapSuppressionController(this)) {
}

TouchscreenTapSuppressionController::~TouchscreenTapSuppressionController() {}

void TouchscreenTapSuppressionController::GestureFlingCancel() {
  controller_->GestureFlingCancel();
}

void TouchscreenTapSuppressionController::GestureFlingCancelAck(
    bool processed) {
  controller_->GestureFlingCancelAck(processed);
}

bool TouchscreenTapSuppressionController::ShouldDeferGestureTapDown(
    const GestureEventWithLatencyInfo& event) {
  bool should_defer = controller_->ShouldDeferTapDown();
  if (should_defer)
    stashed_tap_down_.reset(new GestureEventWithLatencyInfo(event));
  return should_defer;
}

bool TouchscreenTapSuppressionController::ShouldDeferGestureShowPress(
    const GestureEventWithLatencyInfo& event) {
  if (!stashed_tap_down_)
    return false;

  stashed_show_press_.reset(new GestureEventWithLatencyInfo(event));
  return true;
}

bool TouchscreenTapSuppressionController::ShouldSuppressGestureTapEnd() {
  return controller_->ShouldSuppressTapEnd();
}

#if defined(OS_ANDROID)
// TODO(jdduke): Enable ui::GestureConfiguration on Android and initialize
//               with parameters from ViewConfiguration.
int TouchscreenTapSuppressionController::MaxCancelToDownTimeInMs() {
  return gfx::ViewConfiguration::GetTapTimeoutInMs();
}

int TouchscreenTapSuppressionController::MaxTapGapTimeInMs() {
  return gfx::ViewConfiguration::GetLongPressTimeoutInMs();
}
#else
int TouchscreenTapSuppressionController::MaxCancelToDownTimeInMs() {
  return ui::GestureConfiguration::fling_max_cancel_to_down_time_in_ms();
}

int TouchscreenTapSuppressionController::MaxTapGapTimeInMs() {
  return static_cast<int>(
      ui::GestureConfiguration::semi_long_press_time_in_seconds() * 1000);
}
#endif

void TouchscreenTapSuppressionController::DropStashedTapDown() {
  stashed_tap_down_.reset();
  stashed_show_press_.reset();
}

void TouchscreenTapSuppressionController::ForwardStashedTapDown() {
  DCHECK(stashed_tap_down_);
  ScopedGestureEvent tap_down = stashed_tap_down_.Pass();
  ScopedGestureEvent show_press = stashed_show_press_.Pass();
  gesture_event_queue_->ForwardGestureEvent(*tap_down);
  if (show_press)
    gesture_event_queue_->ForwardGestureEvent(*show_press);
}

}  // namespace content
