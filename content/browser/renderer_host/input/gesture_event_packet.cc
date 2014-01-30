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

bool IsTouchSequenceStart(const WebTouchEvent& event) {
  if (event.type != WebInputEvent::TouchStart)
    return false;
  if (!event.touchesLength)
    return false;
  for (size_t i = 0; i < event.touchesLength; i++) {
    if (event.touches[i].state != WebTouchPoint::StatePressed)
      return false;
  }
  return true;
}

GestureEventPacket::GestureSource
ToGestureSource(const WebTouchEvent& event) {
  return IsTouchSequenceStart(event) ? GestureEventPacket::TOUCH_BEGIN
                                     : GestureEventPacket::TOUCH;
}

}  // namespace

GestureEventPacket::GestureEventPacket()
    : gesture_count_(0),
      gesture_source_(INVALID) {}

GestureEventPacket::GestureEventPacket(GestureSource source)
    : gesture_count_(0),
      gesture_source_(source) {
  DCHECK_NE(gesture_source_, INVALID);
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
