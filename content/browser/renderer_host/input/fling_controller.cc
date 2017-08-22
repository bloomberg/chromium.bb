// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/fling_controller.h"

#include "content/browser/renderer_host/input/gesture_event_queue.h"

using blink::WebInputEvent;

namespace content {

FlingController::Config::Config() {}

FlingController::FlingController(
    GestureEventQueue* gesture_event_queue,
    TouchpadTapSuppressionControllerClient* touchpad_client,
    const Config& config)
    : gesture_event_queue_(gesture_event_queue),
      touchpad_tap_suppression_controller_(
          touchpad_client,
          config.touchpad_tap_suppression_config),
      touchscreen_tap_suppression_controller_(
          gesture_event_queue,
          config.touchscreen_tap_suppression_config) {
  DCHECK(gesture_event_queue);
  DCHECK(touchpad_client);
}

FlingController::~FlingController() {}

bool FlingController::ShouldForwardForGFCFiltering(
    const GestureEventWithLatencyInfo& gesture_event) const {
  if (gesture_event.event.GetType() != WebInputEvent::kGestureFlingCancel)
    return true;
  return !gesture_event_queue_->ShouldDiscardFlingCancelEvent(gesture_event);
}

bool FlingController::ShouldForwardForTapSuppression(
    const GestureEventWithLatencyInfo& gesture_event) {
  switch (gesture_event.event.GetType()) {
    case WebInputEvent::kGestureFlingCancel:
      if (gesture_event.event.source_device ==
          blink::kWebGestureDeviceTouchscreen)
        touchscreen_tap_suppression_controller_.GestureFlingCancel();
      else if (gesture_event.event.source_device ==
               blink::kWebGestureDeviceTouchpad)
        touchpad_tap_suppression_controller_.GestureFlingCancel();
      return true;
    case WebInputEvent::kGestureTapDown:
    case WebInputEvent::kGestureShowPress:
    case WebInputEvent::kGestureTapUnconfirmed:
    case WebInputEvent::kGestureTapCancel:
    case WebInputEvent::kGestureTap:
    case WebInputEvent::kGestureDoubleTap:
    case WebInputEvent::kGestureLongPress:
    case WebInputEvent::kGestureLongTap:
    case WebInputEvent::kGestureTwoFingerTap:
      if (gesture_event.event.source_device ==
          blink::kWebGestureDeviceTouchscreen) {
        return !touchscreen_tap_suppression_controller_.FilterTapEvent(
            gesture_event);
      }
      return true;
    default:
      return true;
  }
}

bool FlingController::FilterGestureEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  return (!ShouldForwardForGFCFiltering(gesture_event) ||
          !ShouldForwardForTapSuppression(gesture_event));
}

void FlingController::GestureFlingCancelAck(
    blink::WebGestureDevice source_device,
    bool processed) {
  if (source_device == blink::kWebGestureDeviceTouchscreen)
    touchscreen_tap_suppression_controller_.GestureFlingCancelAck(processed);
  else if (source_device == blink::kWebGestureDeviceTouchpad)
    touchpad_tap_suppression_controller_.GestureFlingCancelAck(processed);
}

TouchpadTapSuppressionController*
FlingController::GetTouchpadTapSuppressionController() {
  return &touchpad_tap_suppression_controller_;
}

}  // namespace content
