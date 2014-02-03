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

void TouchDispositionGestureFilter::OnGestureEventPacket(
    const GestureEventPacket& packet) {
  switch (packet.gesture_source()) {
    case GestureEventPacket::TOUCH_BEGIN:
      sequences_.push(GestureSequence());
      break;

    case GestureEventPacket::TOUCH:
      break;

    case GestureEventPacket::TOUCH_TIMEOUT:
      // Handle the timeout packet immediately if the packet preceding the
      // timeout has already been dispatched.
      if (Tail().IsEmpty()) {
        if (!Tail().IsGesturePrevented())
          SendPacket(packet);
        return;
      }
      break;

    case GestureEventPacket::INVALID:
      NOTREACHED() << "Invalid gesture packet detected.";
      break;
  }

  Tail().Push(packet);
}

void TouchDispositionGestureFilter::OnTouchEventAck(
    InputEventAckState ack_state) {
  if (Head().IsEmpty()) {
    CancelTapIfNecessary();
    CancelFlingIfNecessary();
    sequences_.pop();
  }

  GestureSequence& sequence = Head();
  sequence.UpdateState(ack_state);

  // Dispatch the packet corresponding to the ack'ed touch, as well as any
  // additional timeout-based packets queued before the ack was received.
  bool touch_packet_for_current_ack_handled = false;
  while (!sequence.IsEmpty()) {
    const GestureEventPacket& packet = sequence.Front();

    if (packet.gesture_source() == GestureEventPacket::TOUCH_BEGIN ||
        packet.gesture_source() == GestureEventPacket::TOUCH) {
      // We should handle at most one touch-based packet corresponding to a
      // given ack.
      if (touch_packet_for_current_ack_handled)
        break;
      touch_packet_for_current_ack_handled = true;
    }

    if (!sequence.IsGesturePrevented())
      SendPacket(packet);

    sequence.Pop();
  }
  DCHECK(touch_packet_for_current_ack_handled);

  // Immediately cancel a TapDown if TouchStart went unconsumed, but a
  // subsequent TouchMove is consumed.
  if (sequence.IsGesturePrevented())
    CancelTapIfNecessary();
}

void TouchDispositionGestureFilter::SendPacket(
    const GestureEventPacket& packet) {
  for (size_t i = 0; i < packet.gesture_count(); ++i)
    SendGesture(packet.gesture(i));
}

void TouchDispositionGestureFilter::SendGesture(const WebGestureEvent& event) {
  switch (event.type) {
    case WebInputEvent::GestureLongTap:
      CancelTapIfNecessary();
      CancelFlingIfNecessary();
      break;
    case WebInputEvent::GestureTapDown:
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


// TouchDispositionGestureFilter::GestureSequence

TouchDispositionGestureFilter::GestureSequence::GestureSequence()
    : state_(PENDING) {}

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
    InputEventAckState ack_state) {
  DCHECK_NE(INPUT_EVENT_ACK_STATE_UNKNOWN, ack_state);
  // Permanent states will not be affected by subsequent ack's.
  if (state_ != PENDING && state_ != ALLOWED_UNTIL_PREVENTED)
    return;

  // |NO_CONSUMER| should only be effective when the *first* touch is ack'ed.
  if (state_ == PENDING &&
      ack_state == INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS)
    state_ = ALWAYS_ALLOWED;
  else if (ack_state == INPUT_EVENT_ACK_STATE_CONSUMED)
    state_ = ALWAYS_PREVENTED;
  else
    state_ = ALLOWED_UNTIL_PREVENTED;
}

bool TouchDispositionGestureFilter::GestureSequence::IsGesturePrevented()
    const {
  return state_ == ALWAYS_PREVENTED;
}

bool TouchDispositionGestureFilter::GestureSequence::IsEmpty() const {
  return packets_.empty();
}

}  // namespace content
