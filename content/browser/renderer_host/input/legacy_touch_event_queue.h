// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_LEGACY_TOUCH_EVENT_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_LEGACY_TOUCH_EVENT_QUEUE_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/input/touch_event_queue.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_ack_state.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/gfx/geometry/point_f.h"

namespace content {

class CoalescedWebTouchEvent;
class TouchTimeoutHandler;

// A queue for throttling and coalescing touch-events.
class CONTENT_EXPORT LegacyTouchEventQueue : public TouchEventQueue {
 public:
  // The |client| must outlive the LegacyTouchEventQueue.
  LegacyTouchEventQueue(TouchEventQueueClient* client, const Config& config);

  ~LegacyTouchEventQueue() override;

  // Adds an event to the queue. The event may be coalesced with previously
  // queued events (e.g. consecutive touch-move events can be coalesced into a
  // single touch-move event). The event may also be immediately forwarded to
  // the renderer (e.g. when there are no other queued touch event).
  void QueueEvent(const TouchEventWithLatencyInfo& event) override;

  // Insert a TouchScrollStarted event in the queue ahead of all not-in-flight
  // events.
  void PrependTouchScrollNotification() override;

  // Notifies the queue that a touch-event has been processed by the renderer.
  // At this point, if the ack is for async touchmove, remove the uncancelable
  // touchmove from the front of the queue and decide if it should dispatch the
  // next pending async touch move event, otherwise the queue may send one or
  // more gesture events and/or additional queued touch-events to the renderer.
  void ProcessTouchAck(InputEventAckState ack_result,
                       const ui::LatencyInfo& latency_info,
                       const uint32_t unique_touch_event_id) override;

  // When GestureScrollBegin is received, we send a touch cancel to renderer,
  // route all the following touch events directly to client, and ignore the
  // ack for the touch cancel. When Gesture{ScrollEnd,FlingStart} is received,
  // resume the normal flow of sending touch events to the renderer.
  void OnGestureScrollEvent(
      const GestureEventWithLatencyInfo& gesture_event) override;

  void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                         InputEventAckState ack_result) override;

  // Notifies the queue whether the renderer has at least one touch handler.
  void OnHasTouchEventHandlers(bool has_handlers) override;

  // Returns whether the currently pending touch event (waiting ACK) is for
  // a touch start event.
  bool IsPendingAckTouchStart() const override;

  // Sets whether a delayed touch ack will cancel and flush the current
  // touch sequence. Note that, if the timeout was previously disabled, enabling
  // it will take effect only for the following touch sequence.
  void SetAckTimeoutEnabled(bool enabled) override;

  // Sets whether the current site has a mobile friendly viewport. This
  // determines which ack timeout delay will be used for *future* touch events.
  // The default assumption is that the site is *not* mobile-optimized.
  void SetIsMobileOptimizedSite(bool mobile_optimized_site) override;

  // Whether ack timeout behavior is supported and enabled for the current site.
  bool IsAckTimeoutEnabled() const override;

  bool Empty() const override;

  bool empty() const WARN_UNUSED_RESULT { return Empty(); }

  size_t size() const { return touch_queue_.size(); }

  bool has_handlers() const { return has_handlers_; }

  size_t uncancelable_touch_moves_pending_ack_count() const {
    return ack_pending_async_touchmove_ids_.size();
  }

 private:
  friend class LegacyTouchEventQueueTest;

  bool HasPendingAsyncTouchMoveForTesting() const;
  bool IsTimeoutRunningForTesting() const;
  const TouchEventWithLatencyInfo& GetLatestEventForTesting() const;

  // Empties the queue of touch events. This may result in any number of gesture
  // events being sent to the renderer.
  void FlushQueue() override;

  // Walks the queue, checking each event with |FilterBeforeForwarding()|.
  // If allowed, forwards the touch event and stops processing further events.
  // Otherwise, acks the event with |INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS|.
  void TryForwardNextEventToRenderer();

  // Forwards the event at the head of the queue to the renderer.
  void ForwardNextEventToRenderer();

  // Pops the touch-event from the head of the queue and acks it to the client.
  void PopTouchEventToClient(InputEventAckState ack_result);

  // Pops the touch-event from the top of the queue and acks it to the client,
  // updating the event with |renderer_latency_info|.
  void PopTouchEventToClient(InputEventAckState ack_result,
                             const ui::LatencyInfo& renderer_latency_info);

  // Ack all coalesced events at the head of the queue to the client with
  // |ack_result|, updating the acked events with |optional_latency_info| if it
  // exists, and popping the head of the queue.
  void AckTouchEventToClient(InputEventAckState ack_result,
                             const ui::LatencyInfo* optional_latency_info);

  // Dispatch a touch cancel event for the |event_to_cancel|.
  void SendTouchCancelEventForTouchEvent(
      const TouchEventWithLatencyInfo& event_to_cancel) override;

  // Dispatch |touch| to the client. Before dispatching, updates pointer
  // states in touchmove events for pointers that have not changed position.
  void SendTouchEventImmediately(TouchEventWithLatencyInfo* touch);

  enum PreFilterResult {
    ACK_WITH_NO_CONSUMER_EXISTS,
    ACK_WITH_NOT_CONSUMED,
    FORWARD_TO_RENDERER,
  };
  // Filter touches prior to forwarding to the renderer, e.g., if the renderer
  // has no touch handler.
  PreFilterResult FilterBeforeForwarding(const blink::WebTouchEvent& event);
  void ForwardToRenderer(const TouchEventWithLatencyInfo& event);
  void UpdateTouchConsumerStates(const blink::WebTouchEvent& event,
                                 InputEventAckState ack_result) override;
  void FlushPendingAsyncTouchmove();

  // Handles touch event forwarding and ack'ed event dispatch.
  TouchEventQueueClient* client_;

  std::list<std::unique_ptr<CoalescedWebTouchEvent>> touch_queue_;

  // Used to defer touch forwarding when ack dispatch triggers |QueueEvent()|.
  // True within the scope of |AckTouchEventToClient()|.
  bool dispatching_touch_ack_;

  // Used to prevent touch timeout scheduling and increase the count for async
  // touchmove if we receive a synchronous ack after forwarding a touch event
  // to the client.
  bool dispatching_touch_;

  // Whether the renderer has at least one touch handler.
  bool has_handlers_;

  // Whether any pointer in the touch sequence reported having a consumer.
  bool has_handler_for_current_sequence_;

  // Whether to allow any remaining touches for the current sequence. Note that
  // this is a stricter condition than an empty |touch_consumer_states_|, as it
  // also prevents forwarding of touchstart events for new pointers in the
  // current sequence. This is only used when the event is synthetically
  // cancelled after a touch timeout.
  bool drop_remaining_touches_in_sequence_;

  // Optional handler for timed-out touch event acks.
  std::unique_ptr<TouchTimeoutHandler> timeout_handler_;

  // Whether touch events should remain buffered and dispatched asynchronously
  // while a scroll sequence is active.  In this mode, touchmove's are throttled
  // and ack'ed immediately, but remain buffered in |pending_async_touchmove_|
  // until a sufficient time period has elapsed since the last sent touch event.
  // For details see the design doc at http://goo.gl/lVyJAa.
  bool send_touch_events_async_;
  std::unique_ptr<TouchEventWithLatencyInfo> pending_async_touchmove_;

  // For uncancelable touch moves, not only we send a fake ack, but also a real
  // ack from render, which we use to decide when to send the next async
  // touchmove. This can help avoid the touch event queue keep growing when
  // render handles touchmove slow. We use a queue
  // ack_pending_async_touchmove_ids to store the recent dispatched
  // uncancelable touchmoves which are still waiting for their acks back from
  // render. We do not put them back to the front the touch_event_queue any
  // more.
  base::circular_deque<uint32_t> ack_pending_async_touchmove_ids_;

  double last_sent_touch_timestamp_sec_;

  // Event is saved to compare pointer positions for new touchmove events.
  std::unique_ptr<blink::WebTouchEvent> last_sent_touchevent_;

  DISALLOW_COPY_AND_ASSIGN(LegacyTouchEventQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_LEGACY_TOUCH_EVENT_QUEUE_H_
