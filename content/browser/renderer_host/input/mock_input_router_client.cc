// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mock_input_router_client.h"

#include "content/browser/renderer_host/input/input_router.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebTouchEvent;
using WebKit::WebTouchPoint;

namespace content {

MockInputRouterClient::MockInputRouterClient()
  : input_router_(NULL),
    in_flight_event_count_(0),
    has_touch_handler_(false),
    filter_state_(INPUT_EVENT_ACK_STATE_NOT_CONSUMED),
    is_shortcut_(false),
    allow_send_event_(true),
    send_called_(false),
    send_immediately_called_(false),
    did_flush_called_(false),
    set_needs_flush_called_(false) {}

MockInputRouterClient::~MockInputRouterClient() {}

InputEventAckState MockInputRouterClient::FilterInputEvent(
    const WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info)  {
  return filter_state_;
}

void MockInputRouterClient::IncrementInFlightEventCount()  {
  ++in_flight_event_count_;
}

void MockInputRouterClient::DecrementInFlightEventCount()  {
  --in_flight_event_count_;
}

void MockInputRouterClient::OnHasTouchEventHandlers(
    bool has_handlers)  {
  has_touch_handler_ = has_handlers;
}

bool MockInputRouterClient::OnSendKeyboardEvent(
    const NativeWebKeyboardEvent& key_event,
    const ui::LatencyInfo& latency_info,
    bool* is_shortcut)  {
  send_called_ = true;
  sent_key_event_ = key_event;
  *is_shortcut = is_shortcut_;

  return allow_send_event_;
}

bool MockInputRouterClient::OnSendWheelEvent(
    const MouseWheelEventWithLatencyInfo& wheel_event)  {
  send_called_ = true;
  sent_wheel_event_ = wheel_event;

  return allow_send_event_;
}

bool MockInputRouterClient::OnSendMouseEvent(
    const MouseEventWithLatencyInfo& mouse_event)  {
  send_called_ = true;
  sent_mouse_event_ = mouse_event;

  return allow_send_event_;
}

bool MockInputRouterClient::OnSendTouchEvent(
    const TouchEventWithLatencyInfo& touch_event)  {
  send_called_ = true;
  sent_touch_event_ = touch_event;

  return allow_send_event_;
}

bool MockInputRouterClient::OnSendGestureEvent(
    const GestureEventWithLatencyInfo& gesture_event)  {
  send_called_ = true;
  sent_gesture_event_ = gesture_event;

  return allow_send_event_ &&
         input_router_->ShouldForwardGestureEvent(gesture_event);
}

bool MockInputRouterClient::OnSendMouseEventImmediately(
    const MouseEventWithLatencyInfo& mouse_event)  {
  send_immediately_called_ = true;
  immediately_sent_mouse_event_ = mouse_event;

  return allow_send_event_;
}

bool MockInputRouterClient::OnSendTouchEventImmediately(
    const TouchEventWithLatencyInfo& touch_event)  {
  send_immediately_called_ = true;
  immediately_sent_touch_event_ = touch_event;

  return allow_send_event_;
}

bool MockInputRouterClient::OnSendGestureEventImmediately(
    const GestureEventWithLatencyInfo& gesture_event)  {
  send_immediately_called_ = true;
  immediately_sent_gesture_event_ = gesture_event;
  return allow_send_event_;
}

void MockInputRouterClient::ExpectSendCalled(bool called) {
  EXPECT_EQ(called, send_called_);
  send_called_ = false;
}

void MockInputRouterClient::ExpectSendImmediatelyCalled(bool called) {
  EXPECT_EQ(called, send_immediately_called_);
  send_immediately_called_ = false;
}

void MockInputRouterClient::ExpectNeedsFlushCalled(bool called) {
  EXPECT_EQ(called, set_needs_flush_called_);
  set_needs_flush_called_ = false;
}

void MockInputRouterClient::ExpectDidFlushCalled(bool called) {
  EXPECT_EQ(called, did_flush_called_);
  did_flush_called_ = false;
}

void MockInputRouterClient::DidFlush() {
  did_flush_called_ = true;
}

void MockInputRouterClient::SetNeedsFlush() {
  set_needs_flush_called_ = true;
}

}  // namespace content
