// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_web_input_event_builders.h"

#include "base/logging.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace content {

using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebGestureEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

WebMouseEvent SyntheticWebMouseEventBuilder::Build(
    blink::WebInputEvent::Type type) {
  WebMouseEvent result;
  result.type = type;
  return result;
}

WebMouseEvent SyntheticWebMouseEventBuilder::Build(
    blink::WebInputEvent::Type type,
    int window_x,
    int window_y,
    int modifiers) {
  DCHECK(WebInputEvent::isMouseEventType(type));
  WebMouseEvent result = Build(type);
  result.x = window_x;
  result.y = window_y;
  result.windowX = window_x;
  result.windowY = window_y;
  result.modifiers = modifiers;

  if (type == WebInputEvent::MouseDown || type == WebInputEvent::MouseUp)
    result.button = WebMouseEvent::ButtonLeft;
  else
    result.button = WebMouseEvent::ButtonNone;

  return result;
}

WebMouseWheelEvent SyntheticWebMouseWheelEventBuilder::Build(
    WebMouseWheelEvent::Phase phase) {
  WebMouseWheelEvent result;
  result.type = WebInputEvent::MouseWheel;
  result.phase = phase;
  return result;
}

WebMouseWheelEvent SyntheticWebMouseWheelEventBuilder::Build(float dx,
                                                             float dy,
                                                             int modifiers,
                                                             bool precise) {
  WebMouseWheelEvent result;
  result.type = WebInputEvent::MouseWheel;
  result.deltaX = dx;
  result.deltaY = dy;
  if (dx)
    result.wheelTicksX = dx > 0.0f ? 1.0f : -1.0f;
  if (dy)
    result.wheelTicksY = dy > 0.0f ? 1.0f : -1.0f;
  result.modifiers = modifiers;
  result.hasPreciseScrollingDeltas = precise;
  return result;
}

WebKeyboardEvent SyntheticWebKeyboardEventBuilder::Build(
    WebInputEvent::Type type) {
  DCHECK(WebInputEvent::isKeyboardEventType(type));
  WebKeyboardEvent result;
  result.type = type;
  result.windowsKeyCode = ui::VKEY_L;  // non-null made up value.
  return result;
}

WebGestureEvent SyntheticWebGestureEventBuilder::Build(
    WebInputEvent::Type type,
    WebGestureEvent::SourceDevice source_device) {
  DCHECK(WebInputEvent::isGestureEventType(type));
  WebGestureEvent result;
  result.type = type;
  result.sourceDevice = source_device;
  return result;
}

WebGestureEvent SyntheticWebGestureEventBuilder::BuildScrollBegin(
    float dx_hint,
    float dy_hint) {
  WebGestureEvent result = Build(WebInputEvent::GestureScrollBegin,
                                 WebGestureEvent::Touchscreen);
  result.data.scrollBegin.deltaXHint = dx_hint;
  result.data.scrollBegin.deltaYHint = dy_hint;
  return result;
}

WebGestureEvent SyntheticWebGestureEventBuilder::BuildScrollUpdate(
    float dx,
    float dy,
    int modifiers) {
  WebGestureEvent result = Build(WebInputEvent::GestureScrollUpdate,
                                 WebGestureEvent::Touchscreen);
  result.data.scrollUpdate.deltaX = dx;
  result.data.scrollUpdate.deltaY = dy;
  result.modifiers = modifiers;
  return result;
}

WebGestureEvent SyntheticWebGestureEventBuilder::BuildPinchUpdate(
    float scale,
    float anchor_x,
    float anchor_y,
    int modifiers) {
  WebGestureEvent result = Build(WebInputEvent::GesturePinchUpdate,
                                 WebGestureEvent::Touchscreen);
  result.data.pinchUpdate.scale = scale;
  result.x = anchor_x;
  result.y = anchor_y;
  result.modifiers = modifiers;
  return result;
}

WebGestureEvent SyntheticWebGestureEventBuilder::BuildFling(
    float velocity_x,
    float velocity_y,
    WebGestureEvent::SourceDevice source_device) {
  WebGestureEvent result = Build(WebInputEvent::GestureFlingStart,
                                 source_device);
  result.data.flingStart.velocityX = velocity_x;
  result.data.flingStart.velocityY = velocity_y;
  return result;
}

SyntheticWebTouchEvent::SyntheticWebTouchEvent() : WebTouchEvent() {}

void SyntheticWebTouchEvent::ResetPoints() {
  int point = 0;
  for (unsigned int i = 0; i < touchesLength; ++i) {
    if (touches[i].state == WebTouchPoint::StateReleased)
      continue;

    touches[point] = touches[i];
    touches[point].state = WebTouchPoint::StateStationary;
    ++point;
  }
  touchesLength = point;
  type = WebInputEvent::Undefined;
}

int SyntheticWebTouchEvent::PressPoint(int x, int y) {
  if (touchesLength == touchesLengthCap)
    return -1;
  WebTouchPoint& point = touches[touchesLength];
  point.id = touchesLength;
  point.position.x = point.screenPosition.x = x;
  point.position.y = point.screenPosition.y = y;
  point.state = WebTouchPoint::StatePressed;
  point.radiusX = point.radiusY = 1.f;
  ++touchesLength;
  type = WebInputEvent::TouchStart;
  return point.id;
}

void SyntheticWebTouchEvent::MovePoint(int index, int x, int y) {
  CHECK(index >= 0 && index < touchesLengthCap);
  WebTouchPoint& point = touches[index];
  point.position.x = point.screenPosition.x = x;
  point.position.y = point.screenPosition.y = y;
  touches[index].state = WebTouchPoint::StateMoved;
  type = WebInputEvent::TouchMove;
}

void SyntheticWebTouchEvent::ReleasePoint(int index) {
  CHECK(index >= 0 && index < touchesLengthCap);
  touches[index].state = WebTouchPoint::StateReleased;
  type = WebInputEvent::TouchEnd;
}

void SyntheticWebTouchEvent::CancelPoint(int index) {
  CHECK(index >= 0 && index < touchesLengthCap);
  touches[index].state = WebTouchPoint::StateCancelled;
  type = WebInputEvent::TouchCancel;
}

void SyntheticWebTouchEvent::SetTimestamp(base::TimeDelta timestamp) {
  timeStampSeconds = timestamp.InSecondsF();
}

SyntheticWebTouchEvent SyntheticWebTouchEventBuilder::Build(
    WebInputEvent::Type type) {
  DCHECK(WebInputEvent::isTouchEventType(type));
  SyntheticWebTouchEvent result;
  result.type = type;
  return result;
};

}  // namespace content
