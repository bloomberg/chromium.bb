// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_NON_BLOCKING_EVENT_QUEUE_H_
#define CONTENT_RENDERER_INPUT_NON_BLOCKING_EVENT_QUEUE_H_

#include <deque>
#include "content/common/content_export.h"
#include "content/common/input/event_with_latency_info.h"
#include "content/common/input/web_input_event_queue.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/latency_info.h"

namespace content {

class CONTENT_EXPORT NonBlockingEventQueueClient {
 public:
  // Send an |event| that was previously queued (possibly
  // coalesced with another event) to the |routing_id|'s
  // channel. Implementors must implement this callback.
  virtual void SendNonBlockingEvent(int routing_id,
                                    const blink::WebInputEvent* event,
                                    const ui::LatencyInfo& latency) = 0;
};

// NonBlockingEventQueue implements a series of queues (one touch
// and one mouse wheel) for events that need to be queued between
// the compositor and main threads. When a non-blocking event is sent
// from the compositor to main it can either be sent directly if no
// outstanding events of that type are in flight; or it needs to
// wait in a queue until the main thread has finished processing
// the in-flight event. This class tracks the state and queues
// for the event types. Methods on this class should only be called
// from the compositor thread.
//
class CONTENT_EXPORT NonBlockingEventQueue {
 public:
  NonBlockingEventQueue(int routing_id, NonBlockingEventQueueClient* client);
  ~NonBlockingEventQueue();

  // Called once compositor has handled |event| and indicated that it is
  // a non-blocking event to be queued to the main thread.
  void HandleEvent(const blink::WebInputEvent* event,
                   const ui::LatencyInfo& latency);

  // Call once main thread has handled outstanding |type| event in flight.
  void EventHandled(blink::WebInputEvent::Type type);

 private:
  friend class NonBlockingEventQueueTest;
  int routing_id_;
  NonBlockingEventQueueClient* client_;
  WebInputEventQueue<MouseWheelEventWithLatencyInfo> wheel_events_;
  WebInputEventQueue<TouchEventWithLatencyInfo> touch_events_;

  DISALLOW_COPY_AND_ASSIGN(NonBlockingEventQueue);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_NON_BLOCKING_EVENT_QUEUE_H_
