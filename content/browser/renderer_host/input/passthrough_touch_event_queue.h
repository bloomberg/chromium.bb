// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_PASSTHROUGH_TOUCH_EVENT_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_PASSTHROUGH_TOUCH_EVENT_QUEUE_H_

#include "content/browser/renderer_host/input/touch_event_queue.h"

#include <set>

namespace content {

class TouchTimeoutHandler;

// A queue that processes a touch-event and forwards it on to the
// renderer process immediately. This class assumes that queueing will
// happen inside the renderer process. This class will hold onto the pending
// events so that it can re-order the acks so that they appear in a
// logical order to the rest of the browser process. Due to the threaded
// model of the renderer it is possible that an ack for a touchend can
// be sent before the corresponding ack for the touchstart. This class
// corrects that state.
class CONTENT_EXPORT PassthroughTouchEventQueue : public TouchEventQueue {
 public:
  PassthroughTouchEventQueue(TouchEventQueueClient* client,
                             const Config& config);

  ~PassthroughTouchEventQueue() override;

  // TouchEventQueue overrides.
  void QueueEvent(const TouchEventWithLatencyInfo& event) override;

  void PrependTouchScrollNotification() override;

  void ProcessTouchAck(InputEventAckState ack_result,
                       const ui::LatencyInfo& latency_info,
                       const uint32_t unique_touch_event_id) override;
  void OnGestureScrollEvent(
      const GestureEventWithLatencyInfo& gesture_event) override;

  void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                         InputEventAckState ack_result) override;

  void OnHasTouchEventHandlers(bool has_handlers) override;

  bool IsPendingAckTouchStart() const override;

  void SetAckTimeoutEnabled(bool enabled) override;

  void SetIsMobileOptimizedSite(bool mobile_optimized_site) override;

  bool IsAckTimeoutEnabled() const override;

  bool Empty() const override;

 protected:
  void SendTouchCancelEventForTouchEvent(
      const TouchEventWithLatencyInfo& event_to_cancel) override;
  void UpdateTouchConsumerStates(const blink::WebTouchEvent& event,
                                 InputEventAckState ack_result) override;
  // Empties the queue of touch events. This may result in any number of gesture
  // events being sent to the renderer.
  void FlushQueue() override;

 private:
  friend class PassthroughTouchEventQueueTest;

  class TouchEventWithLatencyInfoAndAckState
      : public TouchEventWithLatencyInfo {
   public:
    TouchEventWithLatencyInfoAndAckState(const TouchEventWithLatencyInfo&);
    bool operator<(const TouchEventWithLatencyInfoAndAckState&) const;
    InputEventAckState ack_state() const { return ack_state_; }
    void set_ack_state(InputEventAckState state) { ack_state_ = state; }

   private:
    InputEventAckState ack_state_;
  };

  enum PreFilterResult {
    ACK_WITH_NO_CONSUMER_EXISTS,
    ACK_WITH_NOT_CONSUMED,
    FORWARD_TO_RENDERER,
  };
  // Filter touches prior to forwarding to the renderer, e.g., if the renderer
  // has no touch handler.
  PreFilterResult FilterBeforeForwarding(const blink::WebTouchEvent& event);

  void AckTouchEventToClient(const TouchEventWithLatencyInfo& acked_event,
                             InputEventAckState ack_result);

  void SendTouchEventImmediately(TouchEventWithLatencyInfo* touch,
                                 bool wait_for_ack);

  void AckCompletedEvents();

  bool IsTimeoutRunningForTesting() const;
  const TouchEventWithLatencyInfo& GetLatestEventForTesting() const;
  size_t SizeForTesting() const;

  // Handles touch event forwarding and ack'ed event dispatch.
  TouchEventQueueClient* client_;

  // Whether the renderer has at least one touch handler.
  bool has_handlers_;

  // Whether any pointer in the touch sequence may have having a consumer.
  bool maybe_has_handler_for_current_sequence_;

  // Whether to allow any remaining touches for the current sequence. Note that
  // this is a stricter condition than an empty |touch_consumer_states_|, as it
  // also prevents forwarding of touchstart events for new pointers in the
  // current sequence. This is only used when the event is synthetically
  // cancelled after a touch timeout.
  bool drop_remaining_touches_in_sequence_;

  // Optional handler for timed-out touch event acks.
  std::unique_ptr<TouchTimeoutHandler> timeout_handler_;

  // Whether touch events should be sent as uncancelable or not.
  bool send_touch_events_async_;

  bool processing_acks_;

  // Event is saved to compare pointer positions for new touchmove events.
  std::unique_ptr<blink::WebTouchEvent> last_sent_touchevent_;

  // Stores outstanding touches that have been sent to the renderer but have
  // not yet been ack'd by the renderer. The set is explicitly ordered based
  // on the unique touch event id.
  std::set<TouchEventWithLatencyInfoAndAckState> outstanding_touches_;

  DISALLOW_COPY_AND_ASSIGN(PassthroughTouchEventQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_PASSTHROUGH_TOUCH_EVENT_QUEUE_H_
