// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TAP_SUPPRESSION_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_TAP_SUPPRESSION_CONTROLLER_H_

#include "base/time.h"
#include "base/timer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

namespace content {

class MockRenderWidgetHost;
class RenderWidgetHostImpl;

// Controls the suppression of taps (rapid mousedown/mouseup sequences)
// immediately following the dispatch of a WebGestureFlingCancel event.
// Only mousedown/mouseup of sufficient speed and within a specified time
// window after a WebGestureFlingCancel are suppressed.
class TapSuppressionController {
 public:

  explicit TapSuppressionController(RenderWidgetHostImpl*);
  ~TapSuppressionController();

  // Called on the arrival of a mouse up event. Returns true if the hosting RWHV
  // should suppress  the remaining mouseup handling at this time.
  bool ShouldSuppressMouseUp();

  // Called on a mouse down. Returns true if the hosting RWHV should not
  // continue with handling the mouse down event at this time.
  bool ShouldDeferMouseDown(
      const WebKit::WebMouseEvent& event);

  // Called on an ack of WebGestureFlingCancel event. |processed| is true when
  // the GestureFlingCancel actually stopped a fling and therefore should
  // suppress the forwarding of the following tap.
  void GestureFlingCancelAck(bool processed);

  // Called on the dispatch of a WebGestureFlingCancel event.
  void GestureFlingCancel(double cancel_time);

 private:
  friend class MockRenderWidgetHost;

   enum State {
     NOTHING,
     GFC_IN_PROGRESS,
     MD_STASHED,
     LAST_CANCEL_STOPPED_FLING,
  };

  // Invoked once the maximum time deemed a tap from a mouse down event
  // has expired. If the mouse up has not yet arrived, indicates that the mouse
  // down / mouse up pair do not form a tap.
  void MouseDownTimerExpired();

  // Only a RenderWidgetHostViewImpl can own an instance.
  RenderWidgetHostImpl* render_widget_host_;
  base::OneShotTimer<TapSuppressionController> mouse_down_timer_;
  WebKit::WebMouseEvent stashed_mouse_down_;
  State state_;

  // TODO(rjkroege): During debugging, the event times did not prove reliable.
  // Replace the use of base::TimeTicks with an accurate event time when they
  // become available post http://crbug.com/119556.
  base::TimeTicks fling_cancel_time_;

  DISALLOW_COPY_AND_ASSIGN(TapSuppressionController);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_TAP_SUPPRESSION_CONTROLLER_H_
