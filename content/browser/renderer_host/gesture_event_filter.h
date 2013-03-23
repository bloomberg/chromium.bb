// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_GESTURE_EVENT_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_GESTURE_EVENT_FILTER_H_

#include <deque>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/gfx/transform.h"

namespace content {
class MockRenderWidgetHost;
class RenderWidgetHostImpl;
class TouchpadTapSuppressionController;
class TouchscreenTapSuppressionController;

// Maintains WebGestureEvents in a queue before forwarding them to the renderer
// to apply a sequence of filters on them:
// 1. Zero-velocity fling-starts from touchpad are filtered.
// 2. The sequence is filtered for bounces. A bounce is when the finger lifts
//    from the screen briefly during an in-progress scroll. If this happens,
//    non-GestureScrollUpdate events are queued until the de-bounce interval
//    passes or another GestureScrollUpdate event occurs.
// 3. Unnecessary GestureFlingCancel events are filtered. These are
//    GestureFlingCancels that have no corresponding GestureFlingStart in the
//    queue.
// 4. Taps immediately after a GestureFlingCancel (caused by the same tap) are
//    filtered.
// 5. Whenever possible, events in the queue are coalesced to have as few events
//    as possible and therefore maximize the chance that the event stream can be
//    handled entirely by the compositor thread.
// Events in the queue are forwarded to the renderer one by one; i.e., each
// event is sent after receiving the ACK for previous one. The only exception is
// that if a GestureScrollUpdate is followed by a GesturePinchUpdate, they are
// sent together.
// TODO(rjkroege): Possibly refactor into a filter chain:
// http://crbug.com/148443.
class GestureEventFilter {
 public:
  explicit GestureEventFilter(RenderWidgetHostImpl*);
  ~GestureEventFilter();

  // Returns |true| if the caller should immediately forward the provided
  // |WebGestureEvent| argument to the renderer.
  bool ShouldForward(const WebKit::WebGestureEvent&);

  // Indicates that the calling RenderWidgetHostImpl has received an
  // acknowledgement from the renderer with state |processed| and event |type|.
  // May send events if the queue is not empty.
  void ProcessGestureAck(bool processed, int type);

  // Resets the state of the filter as would be needed when the renderer exits.
  void Reset();

  // Sets the state of the |fling_in_progress_| field to indicate that a fling
  // is definitely not in progress.
  void FlingHasBeenHalted();

  // Returns the |TouchpadTapSuppressionController| instance.
  TouchpadTapSuppressionController* GetTouchpadTapSuppressionController();

  // Returns whether there are any gesture event in the queue.
  bool HasQueuedGestureEvents() const;

  // Returns the last gesture event that was sent to the renderer.
  const WebKit::WebInputEvent& GetGestureEventAwaitingAck() const;

  // Tries forwarding the event to the tap deferral sub-filter.
  void ForwardGestureEventForDeferral(
      const WebKit::WebGestureEvent& gesture_event);

  // Tries forwarding the event, skipping the tap deferral sub-filter.
  void ForwardGestureEventSkipDeferral(
      const WebKit::WebGestureEvent& gesture_event);

 private:
  friend class MockRenderWidgetHost;

  // TODO(mohsen): There are a bunch of ShouldForward.../ShouldDiscard...
  // methods that are getting confusing. This should be somehow fixed. Maybe
  // while refactoring GEF: http://crbug.com/148443.

  // Invoked on the expiration of the timer to release a deferred
  // GestureTapDown to the renderer.
  void SendGestureTapDownNow();

  // Inovked on the expiration of the debounce interval to release
  // deferred events.
  void SendScrollEndingEventsNow();

  // Returns |true| if the given GestureFlingCancel should be discarded
  // as unnecessary.
  bool ShouldDiscardFlingCancelEvent(
    const WebKit::WebGestureEvent& gesture_event);

  // Returns |true| if the only event in the queue is the current event and
  // hence that event should be handled now.
  bool ShouldHandleEventNow();

  // Merge or append a GestureScrollUpdate or GesturePinchUpdate into
  // the coalescing queue.
  void MergeOrInsertScrollAndPinchEvent(
       const WebKit::WebGestureEvent& gesture_event);

  // Sub-filter for removing zero-velocity fling-starts from touchpad.
  bool ShouldForwardForZeroVelocityFlingStart(
      const WebKit::WebGestureEvent& gesture_event);

  // Sub-filter for removing bounces from in-progress scrolls.
  bool ShouldForwardForBounceReduction(
      const WebKit::WebGestureEvent& gesture_event);

  // Sub-filter for removing unnecessary GestureFlingCancels.
  bool ShouldForwardForGFCFiltering(
      const WebKit::WebGestureEvent& gesture_event);

  // Sub-filter for suppressing taps immediately after a GestureFlingCancel.
  bool ShouldForwardForTapSuppression(
      const WebKit::WebGestureEvent& gesture_event);

  // Sub-filter for deferring GestureTapDowns.
  bool ShouldForwardForTapDeferral(
      const WebKit::WebGestureEvent& gesture_event);

  // Puts the events in a queue to forward them one by one; i.e., forward them
  // whenever ACK for previous event is received. This queue also tries to
  // coalesce events as much as possible.
  bool ShouldForwardForCoalescing(const WebKit::WebGestureEvent& gesture_event);

  // Whether the event_in_queue is GesturePinchUpdate or
  // GestureScrollUpdate and it has the same modifiers as the
  // new event.
  bool ShouldTryMerging(const WebKit::WebGestureEvent& new_event,
      const WebKit::WebGestureEvent& event_in_queue);

  // Returns the transform matrix corresponding to the gesture event.
  // Assumes the gesture event sent is either GestureScrollUpdate or
  // GesturePinchUpdate. Returns the identity matrix otherwise.
  gfx::Transform GetTransformForEvent(
      const WebKit::WebGestureEvent& gesture_event);

  // Only a RenderWidgetHostViewImpl can own an instance.
  RenderWidgetHostImpl* render_widget_host_;

  // True if a GestureFlingStart is in progress on the renderer or
  // queued without a subsequent queued GestureFlingCancel event.
  bool fling_in_progress_;

  // True if a GestureScrollUpdate sequence is in progress.
  bool scrolling_in_progress_;

  // True if two related gesture events were sent before without waiting
  // for an ACK, so the next gesture ACK should be ignored.
  bool ignore_next_ack_;

  // Transform that holds the combined transform matrix for the current
  // scroll-pinch sequence at the end of the queue.
  gfx::Transform combined_scroll_pinch_;

  // Timer to release a previously deferred GestureTapDown event.
  base::OneShotTimer<GestureEventFilter> send_gtd_timer_;

  // An object tracking the state of touchpad on the delivery of mouse events to
  // the renderer to filter mouse immediately after a touchpad fling canceling
  // tap.
  // TODO(mohsen): Move touchpad tap suppression out of GestureEventFilter since
  // GEF is meant to only be used for touchscreen gesture events.
  scoped_ptr<TouchpadTapSuppressionController>
      touchpad_tap_suppression_controller_;

  // An object tracking the state of touchscreen on the delivery of gesture tap
  // events to the renderer to filter taps immediately after a touchscreen fling
  // canceling tap.
  scoped_ptr<TouchscreenTapSuppressionController>
      touchscreen_tap_suppression_controller_;

  typedef std::deque<WebKit::WebGestureEvent> GestureEventQueue;

  // Queue of coalesced gesture events not yet sent to the renderer.
  GestureEventQueue coalesced_gesture_events_;

  // Tap gesture event currently subject to deferral.
  WebKit::WebGestureEvent deferred_tap_down_event_;

  // Timer to release a previously deferred GestureTapDown event.
  base::OneShotTimer<GestureEventFilter> debounce_deferring_timer_;

  // Queue of events that have been deferred for debounce.
  GestureEventQueue debouncing_deferral_queue_;

  // Time window in which to defer a GestureTapDown.
  int maximum_tap_gap_time_ms_;

  // Time window in which to debounce scroll/fling ends.
  // TODO(rjkroege): Make this dynamically configurable.
  int debounce_interval_time_ms_;

  DISALLOW_COPY_AND_ASSIGN(GestureEventFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_GESTURE_EVENT_FILTER_H_
