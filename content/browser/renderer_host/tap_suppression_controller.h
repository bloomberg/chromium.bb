// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TAP_SUPPRESSION_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_TAP_SUPPRESSION_CONTROLLER_H_

#include "base/time.h"
#include "base/timer.h"

namespace content {

class TapSuppressionControllerClient;

// The core controller for suppression of taps (touchpad or touchscreen)
// immediately following a GestureFlingCancel event (caused by the same tap).
// Only taps of sufficient speed and within a specified time window after a
// GestureFlingCancel are suppressed.
class TapSuppressionController {
 public:
  explicit TapSuppressionController(TapSuppressionControllerClient* client);
  virtual ~TapSuppressionController();

  // Should be called whenever a GestureFlingCancel event is received.
  void GestureFlingCancel();

  // Should be called whenever an ACK for a GestureFlingCancel event is
  // received. |processed| is true when the GestureFlingCancel actually stopped
  // a fling and therefore should suppress the forwarding of the following tap.
  // |event_time| is the time ACK was received.
  void GestureFlingCancelAck(bool processed, const base::TimeTicks& event_time);

  // Should be called whenever a tap down (touchpad or touchscreen) is received.
  // Returns true if the tap down should be deferred. The caller is responsible
  // for keeping the event for later release, if needed. |event_time| is the
  // time tap down occured.
  bool ShouldDeferTapDown(const base::TimeTicks& event_time);

  // Should be called whenever a tap up (touchpad or touchscreen) is received.
  // Returns true if the tap up should be suppressed.
  bool ShouldSuppressTapUp();

  // Should be called whenever a tap cancel is received. Returns true if the tap
  // cancel should be suppressed.
  bool ShouldSuppressTapCancel();

 private:
  friend class MockRenderWidgetHost;

  enum State {
    NOTHING,
    GFC_IN_PROGRESS,
    TAP_DOWN_STASHED,
    LAST_CANCEL_STOPPED_FLING,
  };

  void StartTapDownTimer();
  void TapDownTimerExpired();

  TapSuppressionControllerClient* client_;
  base::OneShotTimer<TapSuppressionController> tap_down_timer_;
  State state_;

  // TODO(rjkroege): During debugging, the event times did not prove reliable.
  // Replace the use of base::TimeTicks with an accurate event time when they
  // become available post http://crbug.com/119556.
  base::TimeTicks fling_cancel_time_;

  DISALLOW_COPY_AND_ASSIGN(TapSuppressionController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_TAP_SUPPRESSION_CONTROLLER_H_
