// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/touchscreen_tap_suppression_controller.h"

#include "content/browser/renderer_host/gesture_event_filter.h"
#include "content/browser/renderer_host/tap_suppression_controller.h"
#include "ui/base/gestures/gesture_configuration.h"

namespace content {

TouchscreenTapSuppressionController::TouchscreenTapSuppressionController(
    GestureEventFilter* gef)
    : gesture_event_filter_(gef),
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
    const WebKit::WebGestureEvent& event) {
  bool should_defer = controller_->ShouldDeferTapDown();
  if (should_defer)
    stashed_tap_down_ = event;
  return should_defer;
}

bool TouchscreenTapSuppressionController::ShouldSuppressGestureTap() {
  return controller_->ShouldSuppressTapUp();
}

bool TouchscreenTapSuppressionController::ShouldSuppressGestureTapCancel() {
  return controller_->ShouldSuppressTapCancel();
}

int TouchscreenTapSuppressionController::MaxCancelToDownTimeInMs() {
  return ui::GestureConfiguration::fling_max_cancel_to_down_time_in_ms();
}

int TouchscreenTapSuppressionController::MaxTapGapTimeInMs() {
  return static_cast<int>(
      ui::GestureConfiguration::semi_long_press_time_in_seconds() * 1000);
}

void TouchscreenTapSuppressionController::DropStashedTapDown() {
}

void TouchscreenTapSuppressionController::ForwardStashedTapDownForDeferral() {
  gesture_event_filter_->ForwardGestureEventForDeferral(stashed_tap_down_);
}

void TouchscreenTapSuppressionController::ForwardStashedTapDownSkipDeferral() {
  gesture_event_filter_->ForwardGestureEventSkipDeferral(stashed_tap_down_);
}

}  // namespace content
