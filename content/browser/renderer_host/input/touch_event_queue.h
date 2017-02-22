// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EVENT_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EVENT_QUEUE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_ack_state.h"

namespace content {

// Interface with which TouchEventQueue can forward touch events, and dispatch
// touch event responses.
class CONTENT_EXPORT TouchEventQueueClient {
 public:
  virtual ~TouchEventQueueClient() {}

  virtual void SendTouchEventImmediately(
      const TouchEventWithLatencyInfo& event) = 0;

  virtual void OnTouchEventAck(
      const TouchEventWithLatencyInfo& event,
      InputEventAckState ack_result) = 0;

  virtual void OnFilteringTouchEvent(
      const blink::WebTouchEvent& touch_event) = 0;
};

// A queue for throttling and coalescing touch-events.
class CONTENT_EXPORT TouchEventQueue {
 public:
  struct CONTENT_EXPORT Config {
    Config()
        : desktop_touch_ack_timeout_delay(
              base::TimeDelta::FromMilliseconds(200)),
          mobile_touch_ack_timeout_delay(
              base::TimeDelta::FromMilliseconds(1000)),
          touch_ack_timeout_supported(false) {}

    // Touch ack timeout delay for desktop sites. If zero, timeout behavior
    // is disabled for such sites. Defaults to 200ms.
    base::TimeDelta desktop_touch_ack_timeout_delay;

    // Touch ack timeout delay for mobile sites. If zero, timeout behavior
    // is disabled for such sites. Defaults to 1000ms.
    base::TimeDelta mobile_touch_ack_timeout_delay;

    // Whether the platform supports touch ack timeout behavior.
    // Defaults to false (disabled).
    bool touch_ack_timeout_supported;
  };

  TouchEventQueue() {}

  virtual ~TouchEventQueue() {}

  // Adds an event to the queue. The event may be coalesced with previously
  // queued events (e.g. consecutive touch-move events can be coalesced into a
  // single touch-move event). The event may also be immediately forwarded to
  // the renderer (e.g. when there are no other queued touch event).
  virtual void QueueEvent(const TouchEventWithLatencyInfo& event) = 0;

  // Insert a TouchScrollStarted event in the queue ahead of all not-in-flight
  // events.
  virtual void PrependTouchScrollNotification() = 0;

  // Notifies the queue that a touch-event has been processed by the renderer.
  // At this point, if the ack is for async touchmove, remove the uncancelable
  // touchmove from the front of the queue and decide if it should dispatch the
  // next pending async touch move event, otherwise the queue may send one or
  // more gesture events and/or additional queued touch-events to the renderer.
  virtual void ProcessTouchAck(InputEventAckState ack_result,
                               const ui::LatencyInfo& latency_info,
                               const uint32_t unique_touch_event_id) = 0;

  // When GestureScrollBegin is received, we send a touch cancel to renderer,
  // route all the following touch events directly to client, and ignore the
  // ack for the touch cancel. When Gesture{ScrollEnd,FlingStart} is received,
  // resume the normal flow of sending touch events to the renderer.
  virtual void OnGestureScrollEvent(
      const GestureEventWithLatencyInfo& gesture_event) = 0;

  virtual void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                                 InputEventAckState ack_result) = 0;

  // Notifies the queue whether the renderer has at least one touch handler.
  virtual void OnHasTouchEventHandlers(bool has_handlers) = 0;

  // Returns whether the currently pending touch event (waiting ACK) is for
  // a touch start event.
  virtual bool IsPendingAckTouchStart() const = 0;

  // Sets whether a delayed touch ack will cancel and flush the current
  // touch sequence. Note that, if the timeout was previously disabled, enabling
  // it will take effect only for the following touch sequence.
  virtual void SetAckTimeoutEnabled(bool enabled) = 0;

  // Sets whether the current site has a mobile friendly viewport. This
  // determines which ack timeout delay will be used for *future* touch events.
  // The default assumption is that the site is *not* mobile-optimized.
  virtual void SetIsMobileOptimizedSite(bool mobile_optimized_site) = 0;

  // Whether ack timeout behavior is supported and enabled for the current site.
  virtual bool IsAckTimeoutEnabled() const = 0;

  virtual bool Empty() const WARN_UNUSED_RESULT = 0;

 protected:
  virtual void SendTouchCancelEventForTouchEvent(
      const TouchEventWithLatencyInfo& event_to_cancel) = 0;

  // Empties the queue of touch events. This may result in any number of gesture
  // events being sent to the renderer.
  virtual void FlushQueue() = 0;
  virtual void UpdateTouchConsumerStates(const blink::WebTouchEvent& event,
                                         InputEventAckState ack_result) = 0;

 private:
  friend class TouchTimeoutHandler;

  DISALLOW_COPY_AND_ASSIGN(TouchEventQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EVENT_QUEUE_H_
