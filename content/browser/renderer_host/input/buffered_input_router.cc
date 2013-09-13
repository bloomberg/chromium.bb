// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/buffered_input_router.h"

#include "base/auto_reset.h"
#include "content/browser/renderer_host/input/browser_input_event.h"
#include "content/browser/renderer_host/input/input_ack_handler.h"
#include "content/browser/renderer_host/input/input_queue.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/user_metrics.h"

using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebTouchEvent;

namespace content {

BufferedInputRouter::BufferedInputRouter(IPC::Sender* sender,
                                         InputRouterClient* client,
                                         InputAckHandler* ack_handler,
                                         int routing_id)
    : client_(client),
      ack_handler_(ack_handler),
      sender_(sender),
      routing_id_(routing_id),
      has_touch_handler_(false),
      queued_touch_count_(0),
      input_queue_override_(NULL),
      next_input_id_(1),
      in_flight_packet_id_(0) {
  input_queue_.reset(new InputQueue(this));
}

BufferedInputRouter::~BufferedInputRouter() {}

void BufferedInputRouter::Flush() {
  TRACE_EVENT0("input", "BufferedInputRouter::Flush");
  DCHECK_EQ(0, in_flight_packet_id_);
  input_queue_->BeginFlush();
}

bool BufferedInputRouter::SendInput(scoped_ptr<IPC::Message> message) {
  DCHECK(IPC_MESSAGE_ID_CLASS(message->type()) == InputMsgStart);
  DCHECK(message->type() != InputMsg_HandleEventPacket::ID);
  DCHECK(message->type() != InputMsg_HandleInputEvent::ID);
  input_queue_->QueueEvent(BrowserInputEvent::Create(
      NextInputID(), IPCInputEventPayload::Create(message.Pass()), this));
  return true;
}

void BufferedInputRouter::SendMouseEvent(
    const MouseEventWithLatencyInfo& mouse_event) {
  if (!client_->OnSendMouseEvent(mouse_event))
    return;
  // TODO(jdduke): Coalescing, http://crbug.com/289520
  QueueWebEvent(mouse_event.event, mouse_event.latency, false);
}

void BufferedInputRouter::SendWheelEvent(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  if (!client_->OnSendWheelEvent(wheel_event))
    return;
  QueueWebEvent(wheel_event.event, wheel_event.latency, false);
}

void BufferedInputRouter::SendKeyboardEvent(
    const NativeWebKeyboardEvent& key_event,
    const ui::LatencyInfo& latency_info) {
  bool is_shortcut = false;
  if (!client_->OnSendKeyboardEvent(key_event, latency_info, &is_shortcut))
    return;
  int64 event_id = QueueWebEvent(key_event, latency_info, is_shortcut);
  if (event_id) {
    DCHECK(queued_key_map_.find(event_id) == queued_key_map_.end());
    queued_key_map_[event_id] = key_event;
  }
}

void BufferedInputRouter::SendGestureEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (!client_->OnSendGestureEvent(gesture_event))
    return;
  QueueWebEvent(gesture_event.event, gesture_event.latency, false);
}

void BufferedInputRouter::SendTouchEvent(
    const TouchEventWithLatencyInfo& touch_event) {
  if (!client_->OnSendTouchEvent(touch_event))
    return;
  if (QueueWebEvent(touch_event.event, touch_event.latency, false))
    ++queued_touch_count_;
}

void BufferedInputRouter::SendMouseEventImmediately(
    const MouseEventWithLatencyInfo& mouse_event) {
  if (!client_->OnSendMouseEventImmediately(mouse_event))
    return;
  QueueWebEvent(mouse_event.event, mouse_event.latency, false);
}

void BufferedInputRouter::SendTouchEventImmediately(
    const TouchEventWithLatencyInfo& touch_event) {
  if (!client_->OnSendTouchEventImmediately(touch_event))
    return;
  QueueWebEvent(touch_event.event, touch_event.latency, false);
}

void BufferedInputRouter::SendGestureEventImmediately(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (!client_->OnSendGestureEventImmediately(gesture_event))
    return;
  QueueWebEvent(gesture_event.event, gesture_event.latency, false);
}

void BufferedInputRouter::Deliver(const EventPacket& packet) {
  TRACE_EVENT2("input", "BufferedInputRouter::DeliverPacket",
               "id", packet.id(),
               "events", packet.size());
  DCHECK(packet.id());
  DCHECK(!in_flight_packet_id_);
  if (!sender_->Send(new InputMsg_HandleEventPacket(
           routing_id_, packet, InputEventDispositions()))) {
    return;
  }

  in_flight_packet_id_ = packet.id();
  client_->IncrementInFlightEventCount();
}

void BufferedInputRouter::DidFinishFlush() {
  TRACE_EVENT0("input", "BufferedInputRouter::DidFinishFlush");
  client_->DidFlush();
}

void BufferedInputRouter::SetNeedsFlush() {
  TRACE_EVENT0("input", "BufferedInputRouter::SetNeedsFlush");
  client_->SetNeedsFlush();
}

void BufferedInputRouter::OnDispatched(const BrowserInputEvent& event,
                                       InputEventDisposition disposition) {
  // Only WebInputEvents currently have ack response.
  if (event.payload()->GetType() != InputEvent::Payload::WEB_INPUT_EVENT)
    return;

  const WebInputEventPayload* web_payload =
      WebInputEventPayload::Cast(event.payload());

  OnWebInputEventAck(event.id(),
                     *web_payload->web_event(),
                     web_payload->latency_info(),
                     ToAckState(disposition),
                     true);
}

void BufferedInputRouter::OnDispatched(
    const BrowserInputEvent& event,
    InputEventDisposition disposition,
    ScopedVector<BrowserInputEvent>* followup) {
  DCHECK(followup);
  DCHECK_NE(INPUT_EVENT_ACK_STATE_CONSUMED, ToAckState(disposition));

  // Events sent to the router within this scope will be added to |followup|.
  base::AutoReset<ScopedVector<BrowserInputEvent>*> input_queue_override(
      &input_queue_override_, followup);

  OnDispatched(event, disposition);
}

bool BufferedInputRouter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  bool message_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(BufferedInputRouter, message, message_is_ok)
    IPC_MESSAGE_HANDLER(InputHostMsg_HandleEventPacket_ACK, OnEventPacketAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_HasTouchEventHandlers,
                        OnHasTouchEventHandlers)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!message_is_ok)
    ack_handler_->OnUnexpectedEventAck(InputAckHandler::BAD_ACK_MESSAGE);

  return handled;
}

const NativeWebKeyboardEvent*
    BufferedInputRouter::GetLastKeyboardEvent() const {
  return queued_key_map_.empty() ? NULL : &queued_key_map_.begin()->second;
}

bool BufferedInputRouter::ShouldForwardTouchEvent() const {
  return has_touch_handler_ && queued_touch_count_ > 0;
}

bool BufferedInputRouter::ShouldForwardGestureEvent(
    const GestureEventWithLatencyInfo& touch_event) const {
  return true;
}

void BufferedInputRouter::OnWebInputEventAck(
    int64 event_id,
    const WebKit::WebInputEvent& web_event,
    const ui::LatencyInfo& latency_info,
    InputEventAckState acked_result,
    bool ack_from_input_queue) {
  if (WebInputEvent::isKeyboardEventType(web_event.type)) {
    if (ack_from_input_queue) {
      KeyMap::iterator key_it = queued_key_map_.find(event_id);
      DCHECK(key_it != queued_key_map_.end());
      NativeWebKeyboardEvent key_event = key_it->second;
      queued_key_map_.erase(key_it);
      ack_handler_->OnKeyboardEventAck(key_event, acked_result);
    } else {
      DCHECK_EQ(0, event_id);
      ack_handler_->OnKeyboardEventAck(
          static_cast<const NativeWebKeyboardEvent&>(web_event), acked_result);
    }
    // WARNING: This BufferedInputRouter can be deallocated at this point
    // (i.e.  in the case of Ctrl+W, where the call to
    // HandleKeyboardEvent destroys this BufferedInputRouter).
  } else if (web_event.type == WebInputEvent::MouseWheel) {
    ack_handler_->OnWheelEventAck(
        static_cast<const WebMouseWheelEvent&>(web_event), acked_result);
  } else if (WebInputEvent::isTouchEventType(web_event.type)) {
    if (ack_from_input_queue) {
      DCHECK_GT(queued_touch_count_, 0);
      --queued_touch_count_;
    }
    ack_handler_->OnTouchEventAck(
        TouchEventWithLatencyInfo(static_cast<const WebTouchEvent&>(web_event),
                                  latency_info),
        acked_result);
  } else if (WebInputEvent::isGestureEventType(web_event.type)) {
    ack_handler_->OnGestureEventAck(
        static_cast<const WebGestureEvent&>(web_event), acked_result);
  } else
    NOTREACHED() << "Unexpected WebInputEvent in OnWebInputEventAck";
}

void BufferedInputRouter::OnEventPacketAck(
    int64 packet_id,
    const InputEventDispositions& dispositions) {
  TRACE_EVENT2("input", "BufferedInputRouter::OnEventPacketAck",
               "id", packet_id,
               "dispositions", dispositions.size());
  if (!in_flight_packet_id_ || packet_id != in_flight_packet_id_) {
    ack_handler_->OnUnexpectedEventAck(InputAckHandler::UNEXPECTED_ACK);
    return;
  }

  in_flight_packet_id_ = 0;
  client_->DecrementInFlightEventCount();

  InputQueue::AckResult ack_result =
      input_queue_->OnEventPacketAck(packet_id, dispositions);
  if (ack_result == InputQueue::ACK_UNEXPECTED)
    ack_handler_->OnUnexpectedEventAck(InputAckHandler::UNEXPECTED_ACK);
  else if (ack_result == InputQueue::ACK_INVALID)
    ack_handler_->OnUnexpectedEventAck(InputAckHandler::BAD_ACK_MESSAGE);
}

void BufferedInputRouter::OnHasTouchEventHandlers(bool has_handlers) {
  if (has_touch_handler_ == has_handlers)
    return;
  has_touch_handler_ = has_handlers;
  client_->OnHasTouchEventHandlers(has_handlers);
}

int64 BufferedInputRouter::QueueWebEvent(const WebKit::WebInputEvent& web_event,
                                         const ui::LatencyInfo& latency_info,
                                         bool is_key_shortcut) {
  TRACE_EVENT0("input", "BufferedInputRouter::QueueWebEvent");

  if (FilterWebEvent(web_event, latency_info)) {
    TRACE_EVENT_INSTANT0("input",
                         "BufferedInputRouter::QueueWebEvent::Filtered",
                         TRACE_EVENT_SCOPE_THREAD);
    return 0;
  }

  int64 event_id = NextInputID();
  scoped_ptr<BrowserInputEvent> event = BrowserInputEvent::Create(
      event_id,
      WebInputEventPayload::Create(web_event,
                                   latency_info,
                                   is_key_shortcut),
      this);

  // The presence of |input_queue_override_| implies that we are in the
  // scope of |OnInputEventDispatched()| with an event can create followup.
  if (input_queue_override_)
    input_queue_override_->push_back(event.release());
  else
    input_queue_->QueueEvent(event.Pass());

  return event_id;
}

bool BufferedInputRouter::FilterWebEvent(const WebKit::WebInputEvent& web_event,
                                         const ui::LatencyInfo& latency_info) {
  // Perform optional, synchronous event handling, sending ACK messages for
  // processed events, or proceeding as usual.
  InputEventAckState filter_ack =
      client_->FilterInputEvent(web_event, latency_info);
  switch (filter_ack) {
    // Send the ACK and early exit.
    case INPUT_EVENT_ACK_STATE_CONSUMED:
    case INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS:
      OnWebInputEventAck(0, web_event, latency_info, filter_ack, false);
      // WARNING: |this| may be deleted at this point.
      return true;

    // Drop the event.
    case INPUT_EVENT_ACK_STATE_UNKNOWN:
      return true;

    // Proceed as normal.
    case INPUT_EVENT_ACK_STATE_NOT_CONSUMED:
    default:
      break;
  };

  return false;
}

int64 BufferedInputRouter::NextInputID() { return next_input_id_++; }

}  // namespace content
