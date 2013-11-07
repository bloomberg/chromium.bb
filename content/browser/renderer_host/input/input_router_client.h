// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_CLIENT_H_

#include "content/common/content_export.h"
#include "content/port/browser/event_with_latency_info.h"
#include "content/port/common/input_event_ack_state.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace ui {
struct LatencyInfo;
}

namespace content {

class OverscrollController;

class CONTENT_EXPORT InputRouterClient {
 public:
  virtual ~InputRouterClient() {}

  // Called just prior to events being sent to the renderer, giving the client
  // a chance to perform in-process event filtering.
  // The returned disposition will yield the following behavior:
  //   * |NOT_CONSUMED| will result in |input_event| being sent as usual.
  //   * |CONSUMED| or |NO_CONSUMER_EXISTS| will trigger the appropriate ack.
  //   * |UNKNOWN| will result in |input_event| being dropped.
  virtual InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event,
      const ui::LatencyInfo& latency_info) = 0;

  // Called each time a WebInputEvent IPC is sent.
  virtual void IncrementInFlightEventCount() = 0;

  // Called each time a WebInputEvent ACK IPC is received.
  virtual void DecrementInFlightEventCount() = 0;

  // Called when the renderer notifies that it has touch event handlers.
  virtual void OnHasTouchEventHandlers(bool has_handlers) = 0;

  // Returns an optional OverscrollController.  If non-NULL, the controller
  // will be fed events and event acks by the router, when appropriate.
  // TODO(jdduke): crbug.com/306133 - Move the controller to the router.
  virtual OverscrollController* GetOverscrollController() const = 0;

  // Certain router implementations require periodic flushing of queued events.
  // When this method is called, the client should ensure a timely call, either
  // synchronous or asynchronous, of |Flush| on the InputRouter.
  virtual void SetNeedsFlush() = 0;

  // Called when the router has finished flushing all events queued at the time
  // of the call to Flush.  The call will typically be asynchronous with
  // respect to the call to |Flush| on the InputRouter.
  virtual void DidFlush() = 0;
};

} // namespace content

#endif // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_CLIENT_H_
