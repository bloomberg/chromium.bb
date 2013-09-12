// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/input/input_router_client.h"

namespace content {

class InputRouter;

class MockInputRouterClient : public InputRouterClient {
 public:
  MockInputRouterClient();
  virtual ~MockInputRouterClient();

  // InputRouterClient
  virtual InputEventAckState FilterInputEvent(
      const WebKit::WebInputEvent& input_event,
      const ui::LatencyInfo& latency_info) OVERRIDE;
  virtual void IncrementInFlightEventCount() OVERRIDE;
  virtual void DecrementInFlightEventCount() OVERRIDE;
  virtual void OnHasTouchEventHandlers(bool has_handlers) OVERRIDE;
  virtual bool OnSendKeyboardEvent(
      const NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency_info,
      bool* is_shortcut) OVERRIDE;
  virtual bool OnSendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) OVERRIDE;
  virtual bool OnSendMouseEvent(
      const MouseEventWithLatencyInfo& mouse_event) OVERRIDE;
  virtual bool OnSendTouchEvent(
      const TouchEventWithLatencyInfo& touch_event) OVERRIDE;
  virtual bool OnSendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) OVERRIDE;
  virtual bool OnSendMouseEventImmediately(
      const MouseEventWithLatencyInfo& mouse_event) OVERRIDE;
  virtual bool OnSendTouchEventImmediately(
      const TouchEventWithLatencyInfo& touch_event) OVERRIDE;
  virtual bool OnSendGestureEventImmediately(
      const GestureEventWithLatencyInfo& gesture_event) OVERRIDE;
  virtual void DidFlush() OVERRIDE;
  virtual void SetNeedsFlush() OVERRIDE;

  void ExpectSendCalled(bool called);
  void ExpectSendImmediatelyCalled(bool called);
  void ExpectNeedsFlushCalled(bool called);
  void ExpectDidFlushCalled(bool called);

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
  void set_is_shortcut(bool is_shortcut) {
    is_shortcut_ = is_shortcut;
  }
  void set_allow_send_event(bool allow) {
    allow_send_event_ = allow;
  }
  const NativeWebKeyboardEvent& sent_key_event() {
    return sent_key_event_;
  }
  const MouseWheelEventWithLatencyInfo& sent_wheel_event() {
    return sent_wheel_event_;
  }
  const MouseEventWithLatencyInfo& sent_mouse_event() {
    return sent_mouse_event_;
  }
  const GestureEventWithLatencyInfo& sent_gesture_event() {
    return sent_gesture_event_;
  }
  const MouseEventWithLatencyInfo& immediately_sent_mouse_event() {
    return immediately_sent_mouse_event_;
  }
  const TouchEventWithLatencyInfo& immediately_sent_touch_event() {
    return immediately_sent_touch_event_;
  }
  const GestureEventWithLatencyInfo& immediately_sent_gesture_event() {
    return immediately_sent_gesture_event_;
  }

  bool did_flush_called() const { return did_flush_called_; }
  bool needs_flush_called() const { return set_needs_flush_called_; }
  void set_followup_touch_event(scoped_ptr<GestureEventWithLatencyInfo> event) {
    touch_followup_event_ = event.Pass();
  }

 private:
  InputRouter* input_router_;
  int in_flight_event_count_;
  bool has_touch_handler_;

  InputEventAckState filter_state_;

  bool is_shortcut_;
  bool allow_send_event_;
  bool send_called_;
  NativeWebKeyboardEvent sent_key_event_;
  MouseWheelEventWithLatencyInfo sent_wheel_event_;
  MouseEventWithLatencyInfo sent_mouse_event_;
  TouchEventWithLatencyInfo sent_touch_event_;
  GestureEventWithLatencyInfo sent_gesture_event_;

  bool send_immediately_called_;
  MouseEventWithLatencyInfo immediately_sent_mouse_event_;
  TouchEventWithLatencyInfo immediately_sent_touch_event_;
  GestureEventWithLatencyInfo immediately_sent_gesture_event_;

  bool did_flush_called_;
  bool set_needs_flush_called_;
  scoped_ptr<GestureEventWithLatencyInfo> touch_followup_event_;
};

}  // namespace content

#endif // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_
