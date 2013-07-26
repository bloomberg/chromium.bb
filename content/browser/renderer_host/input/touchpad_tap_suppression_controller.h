// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHPAD_TAP_SUPPRESSION_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHPAD_TAP_SUPPRESSION_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/input/tap_suppression_controller_client.h"
#include "content/port/browser/event_with_latency_info.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

class InputRouter;
class TapSuppressionController;

// Controls the suppression of touchpad taps immediately following the dispatch
// of a GestureFlingCancel event.
class TouchpadTapSuppressionController : public TapSuppressionControllerClient {
 public:
  // The |input_router| must outlive the TouchpadTapSupressionController.
  explicit TouchpadTapSuppressionController(InputRouter* input_router);
  virtual ~TouchpadTapSuppressionController();

  // Should be called on arrival of GestureFlingCancel events.
  void GestureFlingCancel();

  // Should be called on arrival of ACK for a GestureFlingCancel event.
  // |processed| is true if the GestureFlingCancel successfully stopped a fling.
  void GestureFlingCancelAck(bool processed);

  // Should be called on arrival of MouseDown events. Returns true if the caller
  // should stop normal handling of the MouseDown. In this case, the caller is
  // responsible for saving the event for later use, if needed.
  bool ShouldDeferMouseDown(const MouseEventWithLatencyInfo& event);

  // Should be called on arrival of MouseUp events. Returns true if the caller
  // should stop normal handling of the MouseUp.
  bool ShouldSuppressMouseUp();

 private:
  friend class MockRenderWidgetHost;

  // TapSuppressionControllerClient implementation.
  virtual int MaxCancelToDownTimeInMs() OVERRIDE;
  virtual int MaxTapGapTimeInMs() OVERRIDE;
  virtual void DropStashedTapDown() OVERRIDE;
  virtual void ForwardStashedTapDownForDeferral() OVERRIDE;
  virtual void ForwardStashedTapDownSkipDeferral() OVERRIDE;

  InputRouter* input_router_;
  MouseEventWithLatencyInfo stashed_mouse_down_;

  // The core controller of tap suppression.
  scoped_ptr<TapSuppressionController> controller_;

  DISALLOW_COPY_AND_ASSIGN(TouchpadTapSuppressionController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHPAD_TAP_SUPPRESSION_CONTROLLER_H_
