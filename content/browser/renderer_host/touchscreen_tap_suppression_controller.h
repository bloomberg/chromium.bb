// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TOUCHSCREEN_TAP_SUPPRESSION_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_TOUCHSCREEN_TAP_SUPPRESSION_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/tap_suppression_controller_client.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

namespace content {

class GestureEventFilter;
class TapSuppressionController;

// Controls the suppression of touchscreen taps immediately following the
// dispatch of a GestureFlingCancel event.
class TouchscreenTapSuppressionController
    : public TapSuppressionControllerClient {
 public:
  explicit TouchscreenTapSuppressionController(GestureEventFilter* gef);
  virtual ~TouchscreenTapSuppressionController();

  // Should be called on arrival of GestureFlingCancel events.
  void GestureFlingCancel();

  // Should be called on arrival of ACK for a GestureFlingCancel event.
  // |processed| is true if the GestureFlingCancel successfully stopped a fling.
  void GestureFlingCancelAck(bool processed);

  // Should be called on arrival of GestureTapDown events. Returns true if the
  // caller should stop normal handling of the GestureTapDown. In this case, the
  // caller is responsible for saving the event for later use, if needed.
  bool ShouldDeferGestureTapDown(const WebKit::WebGestureEvent& event);

  // Should be called on arrival of GestureTap events. Returns true if the
  // caller should stop normal handling of the GestureTap.
  bool ShouldSuppressGestureTap();

  // Should be called on arrival of GestureTapCancel events. Returns true if the
  // caller should stop normal handling of the GestureTapCancel.
  bool ShouldSuppressGestureTapCancel();

 private:
  // TapSuppressionControllerClient implementation.
  virtual int MaxCancelToDownTimeInMs() OVERRIDE;
  virtual int MaxTapGapTimeInMs() OVERRIDE;
  virtual void DropStashedTapDown() OVERRIDE;
  virtual void ForwardStashedTapDownForDeferral() OVERRIDE;
  virtual void ForwardStashedTapDownSkipDeferral() OVERRIDE;

  GestureEventFilter* gesture_event_filter_;
  WebKit::WebGestureEvent stashed_tap_down_;

  // The core controller of tap suppression.
  scoped_ptr<TapSuppressionController> controller_;

  DISALLOW_COPY_AND_ASSIGN(TouchscreenTapSuppressionController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_TOUCHSCREEN_TAP_SUPPRESSION_CONTROLLER_H_
