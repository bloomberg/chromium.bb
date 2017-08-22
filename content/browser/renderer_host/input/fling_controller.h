// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLING_CONTROLLER_H_
#define FLING_CONTROLLER_H_

#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/browser/renderer_host/input/touchscreen_tap_suppression_controller.h"

namespace content {

class GestureEventQueue;

class FlingController {
 public:
  struct CONTENT_EXPORT Config {
    Config();

    // Controls touchpad-related tap suppression, disabled by default.
    TapSuppressionController::Config touchpad_tap_suppression_config;

    // Controls touchscreen-related tap suppression, disabled by default.
    TapSuppressionController::Config touchscreen_tap_suppression_config;
  };

  FlingController(GestureEventQueue* gesture_event_queue,
                  TouchpadTapSuppressionControllerClient* touchpad_client,
                  const Config& config);

  ~FlingController();

  // Sub-filter for removing unnecessary GestureFlingCancels.
  bool ShouldForwardForGFCFiltering(
      const GestureEventWithLatencyInfo& gesture_event) const;

  // Sub-filter for suppressing taps immediately after a GestureFlingCancel.
  bool ShouldForwardForTapSuppression(
      const GestureEventWithLatencyInfo& gesture_event);

  bool FilterGestureEvent(const GestureEventWithLatencyInfo& gesture_event);

  void GestureFlingCancelAck(blink::WebGestureDevice source_device,
                             bool processed);

  // Returns the |TouchpadTapSuppressionController| instance.
  TouchpadTapSuppressionController* GetTouchpadTapSuppressionController();

 private:
  GestureEventQueue* gesture_event_queue_;

  // An object tracking the state of touchpad on the delivery of mouse events to
  // the renderer to filter mouse immediately after a touchpad fling canceling
  // tap.
  TouchpadTapSuppressionController touchpad_tap_suppression_controller_;

  // An object tracking the state of touchscreen on the delivery of gesture tap
  // events to the renderer to filter taps immediately after a touchscreen fling
  // canceling tap.
  TouchscreenTapSuppressionController touchscreen_tap_suppression_controller_;

  DISALLOW_COPY_AND_ASSIGN(FlingController);
};

}  // namespace content

#endif  // FLING_CONTROLLER_H_
