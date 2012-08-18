// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_GESTURE_EVENT_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_GESTURE_EVENT_FILTER_H_

#include <deque>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
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

  // Invoked on the expiration of the timer to release a deferred
  // GestureTapDown to the renderer.
  void SendGestureTapDownNow();

  // Returns |true| if the given GestureFlingCancel should be discarded
  // as unnecessary.
  bool ShouldDiscardFlingCancelEvent(
    const WebKit::WebGestureEvent& gesture_event);

  // Return true if the only event in the queue is the current event and
  // hence that event should be handled now.
  bool ShouldHandleEventNow();

  // Merge a GestureScrollUpdate into the queue if possible or append
  // it to the queue.
  void MergeOrInsertScrollEvent(
       const WebKit::WebGestureEvent& gesture_event);

  // Only a RenderWidgetHostViewImpl can own an instance.
  RenderWidgetHostImpl* render_widget_host_;

  // True if a GestureFlingStart is in progress on the renderer or
  // queued without a subsequent queued GestureFlingCancel event.
  bool fling_in_progress_;

  // Timer to release a previously deferred GestureTapDown event.
  base::OneShotTimer<GestureEventFilter> send_gtd_timer_;

  // An object tracking the state of touchpad action on the delivery of mouse
  // events to the renderer to filter mouse  immediately after a touchpad
  // fling canceling tap.
  scoped_ptr<TapSuppressionController> tap_suppression_controller_;

  typedef std::deque<WebKit::WebGestureEvent> GestureEventQueue;

  // Queue of coalesced gesture events not yet sent to the renderer.
  GestureEventQueue coalesced_gesture_events_;

  // Tap gesture event currently subject to deferral.
  WebKit::WebGestureEvent deferred_tap_down_event_;

  // Time window in which to defer a GestureTapDown.
  int maximum_tap_gap_time_ms_;

  DISALLOW_COPY_AND_ASSIGN(GestureEventFilter);
};

} // namespace content

#endif // CONTENT_BROWSER_RENDERER_HOST_GESTURE_EVENT_FILTER_H_
