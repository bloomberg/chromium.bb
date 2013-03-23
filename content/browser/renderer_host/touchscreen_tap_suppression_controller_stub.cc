// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/touchscreen_tap_suppression_controller.h"

#include "content/browser/renderer_host/tap_suppression_controller.h"

// This is the stub implementation of TouchscreenTapSuppressionController which
// is used on platforms that do not need tap suppression for touchscreen.

namespace content {

TouchscreenTapSuppressionController::TouchscreenTapSuppressionController(
    GestureEventFilter* /*gef*/)
    : gesture_event_filter_(NULL),
      controller_(NULL) {
}

TouchscreenTapSuppressionController::~TouchscreenTapSuppressionController() {}

void TouchscreenTapSuppressionController::GestureFlingCancel() {}

void TouchscreenTapSuppressionController::GestureFlingCancelAck(
    bool /*processed*/) {
}

bool TouchscreenTapSuppressionController::ShouldDeferGestureTapDown(
    const WebKit::WebGestureEvent& /*event*/) {
  return false;
}

bool TouchscreenTapSuppressionController::ShouldSuppressGestureTap() {
  return false;
}

bool TouchscreenTapSuppressionController::ShouldSuppressGestureTapCancel() {
  return false;
}

int TouchscreenTapSuppressionController::MaxCancelToDownTimeInMs() {
  return 0;
}

int TouchscreenTapSuppressionController::MaxTapGapTimeInMs() {
  return 0;
}

void TouchscreenTapSuppressionController::DropStashedTapDown() {}

void TouchscreenTapSuppressionController::ForwardStashedTapDownForDeferral() {}

void TouchscreenTapSuppressionController::ForwardStashedTapDownSkipDeferral() {}

}  // namespace content
