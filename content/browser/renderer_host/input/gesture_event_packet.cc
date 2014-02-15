// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/gesture_event_packet.h"

#include "base/logging.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {
namespace {

GestureEventPacket::GestureSource ToGestureSource(const WebTouchEvent& event) {
  if (!event.touchesLength)
    return GestureEventPacket::INVALID;
  switch (event.type) {
    case WebInputEvent::TouchStart:
      for (size_t i = 0; i < event.touchesLength; i++) {
        if (event.touches[i].state != WebTouchPoint::StatePressed)
          return GestureEventPacket::TOUCH_BEGIN;
      }
      return GestureEventPacket::TOUCH_SEQUENCE_BEGIN;
    case WebInputEvent::TouchMove:
      return GestureEventPacket::TOUCH_MOVE;
    case WebInputEvent::TouchEnd:
    case WebInputEvent::TouchCancel:
      for (size_t i = 0; i < event.touchesLength; i++) {
        if (event.touches[i].state != WebTouchPoint::StateReleased &&
            event.touches[i].state != WebTouchPoint::StateCancelled) {
          return GestureEventPacket::TOUCH_END;
        }
      }
      return GestureEventPacket::TOUCH_SEQUENCE_END;
    default:
      return GestureEventPacket::INVALID;
  }
}

}  // namespace

GestureEventPacket::GestureEventPacket()
    : gesture_count_(0),
      gesture_source_(UNDEFINED) {}

GestureEventPacket::GestureEventPacket(GestureSource source)
    : gesture_count_(0),
      gesture_source_(source) {
  DCHECK_NE(gesture_source_, UNDEFINED);
}

GestureEventPacket::~GestureEventPacket() {}

void GestureEventPacket::Push(const blink::WebGestureEvent& gesture) {
  CHECK_LT(gesture_count_, static_cast<size_t>(kMaxGesturesPerTouch));
  gestures_[gesture_count_++] = gesture;
}

GestureEventPacket GestureEventPacket::FromTouch(const WebTouchEvent& event) {
  return GestureEventPacket(ToGestureSource(event));
}

GestureEventPacket GestureEventPacket::FromTouchTimeout(
    const WebGestureEvent& event) {
  GestureEventPacket packet(TOUCH_TIMEOUT);
  packet.Push(event);
  return packet;
}

}  // namespace content
