// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_disposition_gesture_filter.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/logging.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {
namespace {

WebGestureEvent CreateGesture(WebInputEvent::Type type) {
  DCHECK(WebInputEvent::isGestureEventType(type));
  WebGestureEvent event;
  event.type = type;
  event.sourceDevice = WebGestureEvent::Touchscreen;
  return event;
}

enum RequiredTouches {
  RT_NONE    = 0,
  RT_START   = 1 << 0,
  RT_CURRENT = 1 << 1,
};

struct DispositionHandlingInfo {
  // A bitwise-OR of |RequiredTouches|.
  int required_touches;
  blink::WebInputEvent::Type antecedent_event_type;

  DispositionHandlingInfo(int required_touches)
      : required_touches(required_touches) {}

  DispositionHandlingInfo(int required_touches,
                          blink::WebInputEvent::Type antecedent_event_type)
      : required_touches(required_touches),
        antecedent_event_type(antecedent_event_type) {}
};

DispositionHandlingInfo Info(int required_touches) {
  return DispositionHandlingInfo(required_touches);
}

DispositionHandlingInfo Info(int required_touches,
                             blink::WebInputEvent::Type antecedent_event_type) {
  return DispositionHandlingInfo(required_touches, antecedent_event_type);
}

// This approach to disposition handling is described at http://goo.gl/5G8PWJ.
DispositionHandlingInfo GetDispositionHandlingInfo(
    blink::WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::GestureTapDown:
      return Info(RT_START);
    case WebInputEvent::GestureTapCancel:
      return Info(RT_START);
    case WebInputEvent::GestureShowPress:
      return Info(RT_START);
    case WebInputEvent::GestureLongPress:
      return Info(RT_START);
    case WebInputEvent::GestureLongTap:
      return Info(RT_START | RT_CURRENT);
    case WebInputEvent::GestureTap:
      return Info(RT_START | RT_CURRENT, WebInputEvent::GestureTapUnconfirmed);
    case WebInputEvent::GestureTwoFingerTap:
      return Info(RT_START | RT_CURRENT);
    case WebInputEvent::GestureTapUnconfirmed:
      return Info(RT_START | RT_CURRENT);
    case WebInputEvent::GestureDoubleTap:
      return Info(RT_START | RT_CURRENT, WebInputEvent::GestureTapUnconfirmed);
    case WebInputEvent::GestureScrollBegin:
      return Info(RT_START | RT_CURRENT);
    case WebInputEvent::GestureScrollUpdate:
      return Info(RT_CURRENT, WebInputEvent::GestureScrollBegin);
    case WebInputEvent::GestureScrollEnd:
      return Info(RT_NONE, WebInputEvent::GestureScrollBegin);
    case WebInputEvent::GestureFlingStart:
      return Info(RT_NONE, WebInputEvent::GestureScrollBegin);
    case WebInputEvent::GestureFlingCancel:
      return Info(RT_NONE, WebInputEvent::GestureFlingStart);
    case WebInputEvent::GesturePinchBegin:
      return Info(RT_START, WebInputEvent::GestureScrollBegin);
    case WebInputEvent::GesturePinchUpdate:
      return Info(RT_CURRENT, WebInputEvent::GesturePinchBegin);
    case WebInputEvent::GesturePinchEnd:
      return Info(RT_NONE, WebInputEvent::GesturePinchBegin);
    default:
      NOTREACHED();
      return Info(RT_NONE);
  }
}

}  // namespace

// TouchDispositionGestureFilter

TouchDispositionGestureFilter::TouchDispositionGestureFilter(
    TouchDispositionGestureFilterClient* client)
    : client_(client),
      needs_tap_ending_event_(false),
      needs_fling_ending_event_(false) {
  DCHECK(client_);
}

TouchDispositionGestureFilter::~TouchDispositionGestureFilter() {}

TouchDispositionGestureFilter::PacketResult
TouchDispositionGestureFilter::OnGestureEventPacket(
    const GestureEventPacket& packet) {
  if (packet.gesture_source() == GestureEventPacket::UNDEFINED ||
      packet.gesture_source() == GestureEventPacket::INVALID)
    return INVALID_PACKET_TYPE;

  if (packet.gesture_source() == GestureEventPacket::TOUCH_SEQUENCE_BEGIN)
    sequences_.push(GestureSequence());

  if (IsEmpty())
    return INVALID_PACKET_ORDER;

  if (packet.gesture_source() == GestureEventPacket::TOUCH_TIMEOUT &&
      Tail().IsEmpty()) {
    // Handle the timeout packet immediately if the packet preceding the timeout
    // has already been dispatched.
    FilterAndSendPacket(
        packet, Tail().state(), INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    return SUCCESS;
  }

  Tail().Push(packet);
  return SUCCESS;
}

void TouchDispositionGestureFilter::OnTouchEventAck(
    InputEventAckState ack_state) {
  // Spurious touch acks from the renderer should not trigger a crash.
  if (IsEmpty() || (Head().IsEmpty() && sequences_.size() == 1))
    return;

  if (Head().IsEmpty()) {
    CancelTapIfNecessary();
    CancelFlingIfNecessary();
    last_event_of_type_dropped_.clear();
    sequences_.pop();
  }

  GestureSequence& sequence = Head();

  // Dispatch the packet corresponding to the ack'ed touch, as well as any
  // additional timeout-based packets queued before the ack was received.
  bool touch_packet_for_current_ack_handled = false;
  while (!sequence.IsEmpty()) {
    const GestureEventPacket& packet = sequence.Front();
    DCHECK_NE(packet.gesture_source(), GestureEventPacket::UNDEFINED);
    DCHECK_NE(packet.gesture_source(), GestureEventPacket::INVALID);

    if (packet.gesture_source() != GestureEventPacket::TOUCH_TIMEOUT) {
      // We should handle at most one non-timeout based packet.
      if (touch_packet_for_current_ack_handled)
        break;
      sequence.UpdateState(packet.gesture_source(), ack_state);
      touch_packet_for_current_ack_handled = true;
    }
    FilterAndSendPacket(packet, sequence.state(), ack_state);
    sequence.Pop();
  }
  DCHECK(touch_packet_for_current_ack_handled);
}

bool TouchDispositionGestureFilter::IsEmpty() const {
  return sequences_.empty();
}

void TouchDispositionGestureFilter::FilterAndSendPacket(
    const GestureEventPacket& packet,
    const GestureSequence::GestureHandlingState& sequence_state,
    InputEventAckState ack_state) {
  for (size_t i = 0; i < packet.gesture_count(); ++i) {
    const blink::WebGestureEvent& gesture = packet.gesture(i);
    if (IsGesturePrevented(gesture.type, ack_state, sequence_state)) {
      last_event_of_type_dropped_.insert(gesture.type);
      CancelTapIfNecessary();
      continue;
    }
    last_event_of_type_dropped_.erase(gesture.type);
    SendGesture(gesture);
  }
}

void TouchDispositionGestureFilter::SendGesture(const WebGestureEvent& event) {
  switch (event.type) {
    case WebInputEvent::GestureLongTap:
      CancelTapIfNecessary();
      CancelFlingIfNecessary();
      break;
    case WebInputEvent::GestureTapDown:
      DCHECK(!needs_tap_ending_event_);
      needs_tap_ending_event_ = true;
      break;
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureTapUnconfirmed:
    case WebInputEvent::GestureDoubleTap:
      needs_tap_ending_event_ = false;
      break;
    case WebInputEvent::GestureScrollBegin:
      CancelTapIfNecessary();
      CancelFlingIfNecessary();
      break;
    case WebInputEvent::GestureFlingStart:
      CancelFlingIfNecessary();
      needs_fling_ending_event_ = true;
      break;
    case WebInputEvent::GestureFlingCancel:
      needs_fling_ending_event_ = false;
      break;
    default:
      break;
  }
  client_->ForwardGestureEvent(event);
}

void TouchDispositionGestureFilter::CancelTapIfNecessary() {
  if (!needs_tap_ending_event_)
    return;

  SendGesture(CreateGesture(WebInputEvent::GestureTapCancel));
  DCHECK(!needs_tap_ending_event_);
}

void TouchDispositionGestureFilter::CancelFlingIfNecessary() {
  if (!needs_fling_ending_event_)
    return;

  SendGesture(CreateGesture(WebInputEvent::GestureFlingCancel));
  DCHECK(!needs_fling_ending_event_);
}

// TouchDispositionGestureFilter::GestureSequence

TouchDispositionGestureFilter::GestureSequence::GestureHandlingState::
    GestureHandlingState()
    : seen_ack(false),
      start_consumed(false),
      no_consumer(false) {}

TouchDispositionGestureFilter::GestureSequence&
TouchDispositionGestureFilter::Head() {
  DCHECK(!sequences_.empty());
  return sequences_.front();
}

TouchDispositionGestureFilter::GestureSequence&
TouchDispositionGestureFilter::Tail() {
  DCHECK(!sequences_.empty());
  return sequences_.back();
}

TouchDispositionGestureFilter::GestureSequence::GestureSequence() {}

TouchDispositionGestureFilter::GestureSequence::~GestureSequence() {}

void TouchDispositionGestureFilter::GestureSequence::Push(
    const GestureEventPacket& packet) {
  packets_.push(packet);
}

void TouchDispositionGestureFilter::GestureSequence::Pop() {
  DCHECK(!IsEmpty());
  packets_.pop();
}

const GestureEventPacket&
TouchDispositionGestureFilter::GestureSequence::Front() const {
  DCHECK(!IsEmpty());
  return packets_.front();
}

void TouchDispositionGestureFilter::GestureSequence::UpdateState(
    GestureEventPacket::GestureSource gesture_source,
    InputEventAckState ack_state) {
  DCHECK_NE(INPUT_EVENT_ACK_STATE_UNKNOWN, ack_state);
  // Permanent states will not be affected by subsequent ack's.
  if (state_.no_consumer || state_.start_consumed)
    return;

  // |NO_CONSUMER| should only be effective when the *first* touch is ack'ed.
  if (!state_.seen_ack &&
      ack_state == INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS) {
    state_.no_consumer = true;
  } else if (ack_state == INPUT_EVENT_ACK_STATE_CONSUMED) {
    if (gesture_source == GestureEventPacket::TOUCH_SEQUENCE_BEGIN ||
        gesture_source == GestureEventPacket::TOUCH_BEGIN) {
      state_.start_consumed = true;
    }
  }
  state_.seen_ack = true;
}

bool TouchDispositionGestureFilter::IsGesturePrevented(
    WebInputEvent::Type gesture_type,
    InputEventAckState current,
    const GestureSequence::GestureHandlingState& state) const {

  if (state.no_consumer)
    return false;

  DispositionHandlingInfo disposition_handling_info =
      GetDispositionHandlingInfo(gesture_type);

  int required_touches = disposition_handling_info.required_touches;
  bool current_consumed = current == INPUT_EVENT_ACK_STATE_CONSUMED;
  if ((required_touches & RT_START && state.start_consumed) ||
      (required_touches & RT_CURRENT && current_consumed) ||
      (last_event_of_type_dropped_.count(
           disposition_handling_info.antecedent_event_type) > 0)) {
    return true;
  }
  return false;
}

bool TouchDispositionGestureFilter::GestureSequence::IsEmpty() const {
  return packets_.empty();
}

}  // namespace content
