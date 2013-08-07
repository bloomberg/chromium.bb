// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_IMMEDIATE_INPUT_ROUTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_IMMEDIATE_INPUT_ROUTER_H_

#include <queue>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/touch_event_queue.h"
#include "content/public/browser/native_web_keyboard_event.h"

namespace ui {
struct LatencyInfo;
}

namespace content {

class GestureEventFilter;
class InputRouterClient;
class RenderProcessHost;
class RenderWidgetHostImpl;

// A default implementation for browser input event routing. Input commands are
// forwarded to the renderer immediately upon receipt.
class CONTENT_EXPORT ImmediateInputRouter
    : public NON_EXPORTED_BASE(InputRouter),
      public NON_EXPORTED_BASE(TouchEventQueueClient) {
 public:
  ImmediateInputRouter(RenderProcessHost* process,
                       InputRouterClient* client,
                       int routing_id);
  virtual ~ImmediateInputRouter();

  // InputRouter
  virtual bool SendInput(IPC::Message* message) OVERRIDE;
  virtual void SendMouseEvent(
      const MouseEventWithLatencyInfo& mouse_event) OVERRIDE;
  virtual void SendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) OVERRIDE;
  virtual void SendKeyboardEvent(
      const NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency_info) OVERRIDE;
  virtual void SendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) OVERRIDE;
  virtual void SendTouchEvent(
      const TouchEventWithLatencyInfo& touch_event) OVERRIDE;
  virtual void SendMouseEventImmediately(
      const MouseEventWithLatencyInfo& mouse_event) OVERRIDE;
  virtual void SendTouchEventImmediately(
      const TouchEventWithLatencyInfo& touch_event) OVERRIDE;
  virtual void SendGestureEventImmediately(
      const GestureEventWithLatencyInfo& gesture_event) OVERRIDE;
  virtual const NativeWebKeyboardEvent* GetLastKeyboardEvent() const OVERRIDE;
  virtual bool ShouldForwardTouchEvent() const OVERRIDE;
  virtual bool ShouldForwardGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) const OVERRIDE;
  virtual bool HasQueuedGestureEvents() const OVERRIDE;

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  GestureEventFilter* gesture_event_filter() {
    return gesture_event_filter_.get();
  }

  TouchEventQueue* touch_event_queue() {
    return touch_event_queue_.get();
  }

private:
  // TouchEventQueueClient
  virtual void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                               InputEventAckState ack_result) OVERRIDE;

  bool SendMoveCaret(IPC::Message* message);
  bool SendSelectRange(IPC::Message* message);
  bool Send(IPC::Message* message);

  // Transmits the given input event an as an IPC::Message. This is an internal
  // helper for |FilterAndSendInputEvent()| and should not be used otherwise.
  void SendWebInputEvent(const WebKit::WebInputEvent& input_event,
                         const ui::LatencyInfo& latency_info,
                         bool is_keyboard_shortcut);

  // Filters and forwards the given WebInputEvent to |SendWebInputEvent()|. This
  // is an internal helper for |Send*Event()| and should not be used otherwise.
  void FilterAndSendWebInputEvent(const WebKit::WebInputEvent& input_event,
                                  const ui::LatencyInfo& latency_info,
                                  bool is_keyboard_shortcut);

  // IPC message handlers
  void OnInputEventAck(WebKit::WebInputEvent::Type event_type,
                       InputEventAckState ack_result,
                       const ui::LatencyInfo& latency_info);
  void OnMsgMoveCaretAck();
  void OnSelectRangeAck();
  void OnHasTouchEventHandlers(bool has_handlers);

  // Handle the event ack. Triggered via |OnInputEventAck()| if the event was
  // processed in the renderer, or synchonously from |FilterAndSendInputevent()|
  // if the event was filtered by the |client_| prior to sending.
  void ProcessInputEventAck(WebKit::WebInputEvent::Type event_type,
                            InputEventAckState ack_result,
                            const ui::LatencyInfo& latency_info);

  // Called by ProcessInputEventAck() to process a keyboard event ack message.
  void ProcessKeyboardAck(int type, InputEventAckState ack_result);

  // Called by ProcessInputEventAck() to process a wheel event ack message.
  // This could result in a task being posted to allow additional wheel
  // input messages to be coalesced.
  void ProcessWheelAck(InputEventAckState ack_result);

  // Called by ProcessInputEventAck() to process a gesture event ack message.
  // This validates the gesture for suppression of touchpad taps and sends one
  // previously queued coalesced gesture if it exists.
  void ProcessGestureAck(int type, InputEventAckState ack_result);

  // Called on ProcessInputEventAck() to process a touch event ack message.
  // This can result in a gesture event being generated and sent back to the
  // renderer.
  void ProcessTouchAck(InputEventAckState ack_result,
                       const ui::LatencyInfo& latency_info);

  int routing_id() const { return routing_id_; }


  RenderProcessHost* process_;
  InputRouterClient* client_;
  int routing_id_;

  // (Similar to |mouse_move_pending_|.) True while waiting for SelectRange_ACK.
  bool select_range_pending_;

  // (Similar to |next_mouse_move_|.) The next SelectRange to send, if any.
  scoped_ptr<IPC::Message> next_selection_range_;

  // (Similar to |mouse_move_pending_|.) True while waiting for MoveCaret_ACK.
  bool move_caret_pending_;

  // (Similar to |next_mouse_move_|.) The next MoveCaret to send, if any.
  scoped_ptr<IPC::Message> next_move_caret_;

  // True if a mouse move event was sent to the render view and we are waiting
  // for a corresponding InputHostMsg_HandleInputEvent_ACK message.
  bool mouse_move_pending_;

  // The next mouse move event to send (only non-null while mouse_move_pending_
  // is true).
  scoped_ptr<MouseEventWithLatencyInfo> next_mouse_move_;

  // (Similar to |mouse_move_pending_|.) True if a mouse wheel event was sent
  // and we are waiting for a corresponding ack.
  bool mouse_wheel_pending_;
  MouseWheelEventWithLatencyInfo current_wheel_event_;

  typedef std::deque<MouseWheelEventWithLatencyInfo> WheelEventQueue;

  // (Similar to |next_mouse_move_|.) The next mouse wheel events to send.
  // Unlike mouse moves, mouse wheel events received while one is pending are
  // coalesced (by accumulating deltas) if they match the previous event in
  // modifiers. On the Mac, in particular, mouse wheel events are received at a
  // high rate; not waiting for the ack results in jankiness, and using the same
  // mechanism as for mouse moves (just dropping old events when multiple ones
  // would be queued) results in very slow scrolling.
  WheelEventQueue coalesced_mouse_wheel_events_;

  // The time when an input event was sent to the RenderWidget.
  base::TimeTicks input_event_start_time_;

  // Queue of keyboard events that we need to track.
  typedef std::deque<NativeWebKeyboardEvent> KeyQueue;

  // A queue of keyboard events. We can't trust data from the renderer so we
  // stuff key events into a queue and pop them out on ACK, feeding our copy
  // back to whatever unhandled handler instead of the returned version.
  KeyQueue key_queue_;

  // Keeps track of whether the webpage has any touch event handler. If it does,
  // then touch events are sent to the renderer. Otherwise, the touch events are
  // not sent to the renderer.
  bool has_touch_handler_;

  scoped_ptr<TouchEventQueue> touch_event_queue_;
  scoped_ptr<GestureEventFilter> gesture_event_filter_;

  DISALLOW_COPY_AND_ASSIGN(ImmediateInputRouter);
};

}  // namespace content

#endif // CONTENT_BROWSER_RENDERER_HOST_INPUT_IMMEDIATE_INPUT_ROUTER_H_
