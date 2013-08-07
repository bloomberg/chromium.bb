// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EVENT_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EVENT_QUEUE_H_

#include <deque>
#include <map>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/port/browser/event_with_latency_info.h"
#include "content/port/common/input_event_ack_state.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

class CoalescedWebTouchEvent;

// Interface with which TouchEventQueue can forward touch events, and dispatch
// touch event responses.
class TouchEventQueueClient {
 public:
  virtual ~TouchEventQueueClient() {}

  virtual void SendTouchEventImmediately(
      const TouchEventWithLatencyInfo& event) = 0;

  virtual void OnTouchEventAck(
      const TouchEventWithLatencyInfo& event,
      InputEventAckState ack_result) = 0;
};

// A queue for throttling and coalescing touch-events.
class TouchEventQueue {
 public:

  // The |client| must outlive the TouchEventQueue.
  explicit TouchEventQueue(TouchEventQueueClient* client);
  virtual ~TouchEventQueue();

  // Adds an event to the queue. The event may be coalesced with previously
  // queued events (e.g. consecutive touch-move events can be coalesced into a
  // single touch-move event). The event may also be immediately forwarded to
  // the renderer (e.g. when there are no other queued touch event).
  void QueueEvent(const TouchEventWithLatencyInfo& event);

  // Notifies the queue that a touch-event has been processed by the renderer.
  // At this point, the queue may send one or more gesture events and/or
  // additional queued touch-events to the renderer.
  void ProcessTouchAck(InputEventAckState ack_result,
                       const ui::LatencyInfo& latency_info);

  // Empties the queue of touch events. This may result in any number of gesture
  // events being sent to the renderer.
  void FlushQueue();

  // Returns whether the event-queue is empty.
  bool empty() const WARN_UNUSED_RESULT {
    return touch_queue_.empty();
  }

 private:
  friend class MockRenderWidgetHost;
  friend class ImmediateInputRouterTest;

  CONTENT_EXPORT size_t GetQueueSize() const;
  CONTENT_EXPORT const TouchEventWithLatencyInfo& GetLatestEvent() const;

  // Pops the touch-event from the top of the queue and sends it to the
  // TouchEventQueueClient. This reduces the size of the queue by one.
  void PopTouchEventToClient(InputEventAckState ack_result,
                             const ui::LatencyInfo& renderer_latency_info);

  bool ShouldForwardToRenderer(const WebKit::WebTouchEvent& event) const;

  // Handles touch event forwarding and ack'ed event dispatch.
  TouchEventQueueClient* client_;

  typedef std::deque<CoalescedWebTouchEvent*> TouchQueue;
  TouchQueue touch_queue_;

  // Maintain the ACK status for each touch point.
  typedef std::map<int, InputEventAckState> TouchPointAckStates;
  TouchPointAckStates touch_ack_states_;

  // Used to defer touch forwarding when ack dispatch triggers |QueueEvent()|.
  bool dispatching_touch_ack_;

  DISALLOW_COPY_AND_ASSIGN(TouchEventQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EVENT_QUEUE_H_
