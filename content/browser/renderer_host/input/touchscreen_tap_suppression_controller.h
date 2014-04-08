// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHSCREEN_TAP_SUPPRESSION_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHSCREEN_TAP_SUPPRESSION_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/tap_suppression_controller_client.h"

namespace content {

class GestureEventQueue;
class TapSuppressionController;

// Controls the suppression of touchscreen taps immediately following the
// dispatch of a GestureFlingCancel event.
class TouchscreenTapSuppressionController
    : public TapSuppressionControllerClient {
 public:
  explicit TouchscreenTapSuppressionController(GestureEventQueue* geq);
  virtual ~TouchscreenTapSuppressionController();

  // Should be called on arrival of GestureFlingCancel events.
  void GestureFlingCancel();

  // Should be called on arrival of ACK for a GestureFlingCancel event.
  // |processed| is true if the GestureFlingCancel successfully stopped a fling.
  void GestureFlingCancelAck(bool processed);

  // Should be called on arrival of GestureTapDown events. Returns true if the
  // caller should stop normal handling of the GestureTapDown.
  bool ShouldDeferGestureTapDown(const GestureEventWithLatencyInfo& event);

  // Should be called on arrival of GestureShowPress events. Returns true if the
  // caller should stop normal handling of the GestureShowPress.
  bool ShouldDeferGestureShowPress(const GestureEventWithLatencyInfo& event);

  // Should be called on arrival of tap-ending gesture events. Returns true if
  // the caller should stop normal handling of the GestureTap.
  bool ShouldSuppressGestureTapEnd();

 private:
  // TapSuppressionControllerClient implementation.
  virtual int MaxCancelToDownTimeInMs() OVERRIDE;
  virtual int MaxTapGapTimeInMs() OVERRIDE;
  virtual void DropStashedTapDown() OVERRIDE;
  virtual void ForwardStashedTapDown() OVERRIDE;

  GestureEventQueue* gesture_event_queue_;

  typedef scoped_ptr<GestureEventWithLatencyInfo> ScopedGestureEvent;
  ScopedGestureEvent stashed_tap_down_;
  ScopedGestureEvent stashed_show_press_;

  // The core controller of tap suppression.
  scoped_ptr<TapSuppressionController> controller_;

  DISALLOW_COPY_AND_ASSIGN(TouchscreenTapSuppressionController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHSCREEN_TAP_SUPPRESSION_CONTROLLER_H_
