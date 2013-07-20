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
      const WebKit::WebInputEvent& input_event,
      const ui::LatencyInfo& latency_info) = 0;

  // Called each time a WebInputEvent IPC is sent.
  virtual void IncrementInFlightEventCount() = 0;

  // Called each time a WebInputEvent ACK IPC is received.
  virtual void DecrementInFlightEventCount() = 0;

  // Called when the renderer notifies that it has touch event handlers.
  virtual void OnHasTouchEventHandlers(bool has_handlers) = 0;

  // Called upon Send*Event. Should return true if the event should be sent, and
  // false if the event should be dropped.
  virtual bool OnSendKeyboardEvent(
      const NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency_info,
      bool* is_shortcut) = 0;
  virtual bool OnSendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) = 0;
  virtual bool OnSendMouseEvent(
      const MouseEventWithLatencyInfo& mouse_event) = 0;
  virtual bool OnSendTouchEvent(
      const TouchEventWithLatencyInfo& touch_event) = 0;
  virtual bool OnSendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) = 0;
  virtual bool OnSendMouseEventImmediately(
      const MouseEventWithLatencyInfo& mouse_event) = 0;
  virtual bool OnSendTouchEventImmediately(
      const TouchEventWithLatencyInfo& touch_event) = 0;
  virtual bool OnSendGestureEventImmediately(
      const GestureEventWithLatencyInfo& gesture_event) = 0;

  // Called upon event ack receipt from the renderer.
  virtual void OnKeyboardEventAck(const NativeWebKeyboardEvent& event,
                                  InputEventAckState ack_result) = 0;
  virtual void OnWheelEventAck(const WebKit::WebMouseWheelEvent& event,
                               InputEventAckState ack_result) = 0;
  virtual void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                               InputEventAckState ack_result) = 0;
  virtual void OnGestureEventAck(const WebKit::WebGestureEvent& event,
                                 InputEventAckState ack_result) = 0;
  virtual void OnUnexpectedEventAck(bool bad_message) = 0;
};

} // namespace content

#endif // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_CLIENT_H_
