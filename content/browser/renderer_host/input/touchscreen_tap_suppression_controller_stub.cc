// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touchscreen_tap_suppression_controller.h"

#include "content/browser/renderer_host/input/tap_suppression_controller.h"

// This is the stub implementation of TouchscreenTapSuppressionController which
// is used on platforms that do not need tap suppression for touchscreen.

namespace content {

TouchscreenTapSuppressionController::TouchscreenTapSuppressionController(
    GestureEventQueue* /*geq*/)
    : gesture_event_queue_(NULL) {}

TouchscreenTapSuppressionController::~TouchscreenTapSuppressionController() {}

void TouchscreenTapSuppressionController::GestureFlingCancel() {}

void TouchscreenTapSuppressionController::GestureFlingCancelAck(
    bool /*processed*/) {
}

bool TouchscreenTapSuppressionController::ShouldDeferGestureTapDown(
    const GestureEventWithLatencyInfo& /*event*/) {
  return false;
}

bool TouchscreenTapSuppressionController::ShouldDeferGestureShowPress(
    const GestureEventWithLatencyInfo& /*event*/) {
  return false;
}

bool TouchscreenTapSuppressionController::ShouldSuppressGestureTapEnd() {
  return false;
}

int TouchscreenTapSuppressionController::MaxCancelToDownTimeInMs() {
  return 0;
}

int TouchscreenTapSuppressionController::MaxTapGapTimeInMs() {
  return 0;
}

void TouchscreenTapSuppressionController::DropStashedTapDown() {}

void TouchscreenTapSuppressionController::ForwardStashedTapDown() {}

}  // namespace content
