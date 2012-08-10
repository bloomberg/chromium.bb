// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_GESTURE_EVENT_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_GESTURE_EVENT_FILTER_H_

#include <deque>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

class MockRenderWidgetHost;

namespace content {

class RenderWidgetHostImpl;
class TapSuppressionController;

// Manages a queue of in-progress |WebGestureEvent| instances filtering,
// coalescing or discarding them as required to maximize the chance that
// the event stream can be handled entirely by the compositor thread.
class GestureEventFilter {
 public:
  explicit GestureEventFilter(RenderWidgetHostImpl*);
  ~GestureEventFilter();

  // Returns |true| if the caller should immediately forward the provided
  // |WebGestureEvent| argument to the renderer.
  bool ShouldForward(const WebKit::WebGestureEvent&);

  // Indicate that calling RenderWidgetHostImpl has received an acknowledgement
  // from the renderer with state |processed| and event |type|. May send events
  // if the queue is not empty.
  void ProcessGestureAck(bool processed, int type);

  // Reset the state of the filter as would be needed when the Renderer exits.
  void Reset();

  // Set the state of the |fling_in_progress_| field to indicate that a fling is
  // definitely not in progress.
  void FlingHasBeenHalted();

  // Return the |TapSuppressionController| instance.
  TapSuppressionController* GetTapSuppressionController();

 private:
  friend class ::MockRenderWidgetHost;

  // Returns |true| if the given GestureFlingCancel should be discarded
  // as unnecessary.
  bool ShouldDiscardFlingCancelEvent(
    const WebKit::WebGestureEvent& gesture_event);

  // Only a RenderWidgetHostViewImpl can own an instance.
  RenderWidgetHostImpl* render_widget_host_;

  // True if a GestureFlingStart is in progress on the renderer.
  bool fling_in_progress_;

  // (Similar to |mouse_wheel_pending_|.). True if gesture event was sent and
  // we are waiting for a corresponding ack.
  bool gesture_event_pending_;

  // An object tracking the state of touchpad action on the delivery of mouse
  // events to the renderer to filter mouse actiosn immediately after a touchpad
  // fling canceling tap.
  scoped_ptr<TapSuppressionController> tap_suppression_controller_;

  typedef std::deque<WebKit::WebGestureEvent> GestureEventQueue;

  // (Similar to |coalesced_mouse_wheel_events_|.) GestureScrollUpdate events
  // are coalesced by merging deltas in a similar fashion as wheel events.
  GestureEventQueue coalesced_gesture_events_;

  DISALLOW_COPY_AND_ASSIGN(GestureEventFilter);
};

} // namespace content

#endif // CONTENT_BROWSER_RENDERER_HOST_GESTURE_EVENT_FILTER_H_
