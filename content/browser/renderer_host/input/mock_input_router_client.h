// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_

#include <stddef.h>

#include <memory>

#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/common/input/input_event.h"
#include "ui/events/blink/did_overscroll_params.h"

namespace content {

class InputRouter;

class MockInputRouterClient : public InputRouterClient {
 public:
  MockInputRouterClient();
  ~MockInputRouterClient() override;

  // InputRouterClient
  InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event,
      const ui::LatencyInfo& latency_info) override;
  void IncrementInFlightEventCount(
      blink::WebInputEvent::Type event_type) override;
  void DecrementInFlightEventCount(InputEventAckSource ack_source) override;
  void OnHasTouchEventHandlers(bool has_handlers) override;
  void DidOverscroll(const ui::DidOverscrollParams& params) override;
  void OnSetWhiteListedTouchAction(cc::TouchAction touch_action) override;
  void DidStopFlinging() override;
  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& latency_info) override;

  bool GetAndResetFilterEventCalled();
  ui::DidOverscrollParams GetAndResetOverscroll();
  cc::TouchAction GetAndResetWhiteListedTouchAction();

  void set_input_router(InputRouter* input_router) {
    input_router_ = input_router;
  }

  bool has_touch_handler() const { return has_touch_handler_; }
  void set_filter_state(InputEventAckState filter_state) {
    filter_state_ = filter_state;
  }
  int in_flight_event_count() const {
    return in_flight_event_count_;
  }
  void set_allow_send_event(bool allow) {
    filter_state_ = INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
  }
  const blink::WebInputEvent* last_filter_event() const {
    return last_filter_event_->web_event.get();
  }

 private:
  InputRouter* input_router_;
  int in_flight_event_count_;
  bool has_touch_handler_;

  InputEventAckState filter_state_;

  bool filter_input_event_called_;
  std::unique_ptr<InputEvent> last_filter_event_;

  ui::DidOverscrollParams overscroll_;

  cc::TouchAction white_listed_touch_action_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_
