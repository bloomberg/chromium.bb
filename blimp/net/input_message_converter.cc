// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/input_message_converter.h"

#include "base/logging.h"
#include "blimp/common/proto/input.pb.h"
#include "third_party/WebKit/public/platform/WebGestureDevice.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace blimp {
namespace {

scoped_ptr<blink::WebGestureEvent> BuildCommonWebGesture(
    const InputMessage& proto,
    blink::WebInputEvent::Type type) {
  scoped_ptr<blink::WebGestureEvent> event(new blink::WebGestureEvent);
  event->type = type;
  event->timeStampSeconds = proto.timestamp_seconds();

  const GestureCommon& common = proto.gesture_common();
  event->x = common.x();
  event->y = common.y();
  event->globalX = common.global_x();
  event->globalY = common.global_y();
  event->sourceDevice = blink::WebGestureDevice::WebGestureDeviceTouchscreen;
  return event;
}

scoped_ptr<blink::WebGestureEvent> ProtoToGestureScrollBegin(
    const InputMessage& proto) {
  scoped_ptr<blink::WebGestureEvent> event(
      BuildCommonWebGesture(proto,
                            blink::WebInputEvent::Type::GestureScrollBegin));

  const GestureScrollBegin& details = proto.gesture_scroll_begin();
  event->data.scrollBegin.deltaXHint = details.delta_x_hint();
  event->data.scrollBegin.deltaYHint = details.delta_y_hint();
  event->data.scrollBegin.targetViewport = details.target_viewport();

  return event;
}

scoped_ptr<blink::WebGestureEvent> ProtoToGestureScrollEnd(
    const InputMessage& proto) {
  return BuildCommonWebGesture(proto,
                               blink::WebInputEvent::Type::GestureScrollEnd);
}

scoped_ptr<blink::WebGestureEvent> ProtoToGestureScrollUpdate(
    const InputMessage& proto) {
  scoped_ptr<blink::WebGestureEvent> event(
        BuildCommonWebGesture(proto,
                              blink::WebInputEvent::Type::GestureScrollUpdate));

  const GestureScrollUpdate& details = proto.gesture_scroll_update();
  event->data.scrollUpdate.deltaX = details.delta_x();
  event->data.scrollUpdate.deltaY = details.delta_y();
  event->data.scrollUpdate.velocityX = details.velocity_x();
  event->data.scrollUpdate.velocityY = details.velocity_y();
  event->data.scrollUpdate.previousUpdateInSequencePrevented =
      details.previous_update_in_sequence_prevented();
  event->data.scrollUpdate.preventPropagation = details.prevent_propagation();
  event->data.scrollUpdate.inertial = details.inertial();

  return event;
}

scoped_ptr<blink::WebGestureEvent> ProtoToGestureFlingStart(
    const InputMessage& proto) {
  scoped_ptr<blink::WebGestureEvent> event(
        BuildCommonWebGesture(proto,
                              blink::WebInputEvent::Type::GestureFlingStart));

  const GestureFlingStart& details = proto.gesture_fling_start();
  event->data.flingStart.velocityX = details.velocity_x();
  event->data.flingStart.velocityY = details.velocity_y();
  event->data.flingStart.targetViewport = details.target_viewport();

  return event;
}

scoped_ptr<blink::WebGestureEvent> ProtoToGestureFlingCancel(
    const InputMessage& proto) {
  scoped_ptr<blink::WebGestureEvent> event(
        BuildCommonWebGesture(proto,
                              blink::WebInputEvent::Type::GestureFlingCancel));

  const GestureFlingCancel& details = proto.gesture_fling_cancel();
  event->data.flingCancel.preventBoosting = details.prevent_boosting();

  return event;
}

scoped_ptr<blink::WebGestureEvent> ProtoToGestureTap(
    const InputMessage& proto) {
  scoped_ptr<blink::WebGestureEvent> event(
        BuildCommonWebGesture(proto,
                              blink::WebInputEvent::Type::GestureTap));

  const GestureTap& details = proto.gesture_tap();
  event->data.tap.tapCount = details.tap_count();
  event->data.tap.width = details.width();
  event->data.tap.height = details.height();

  return event;
}

scoped_ptr<blink::WebGestureEvent> ProtoToGesturePinchBegin(
    const InputMessage& proto) {
  return BuildCommonWebGesture(proto,
                               blink::WebInputEvent::Type::GesturePinchBegin);
}

scoped_ptr<blink::WebGestureEvent> ProtoToGesturePinchEnd(
    const InputMessage& proto) {
  return BuildCommonWebGesture(proto,
                               blink::WebInputEvent::Type::GesturePinchEnd);
}

scoped_ptr<blink::WebGestureEvent> ProtoToGesturePinchUpdate(
    const InputMessage& proto) {
  scoped_ptr<blink::WebGestureEvent> event(
        BuildCommonWebGesture(proto,
                              blink::WebInputEvent::Type::GesturePinchUpdate));

  const GesturePinchUpdate& details = proto.gesture_pinch_update();
  event->data.pinchUpdate.zoomDisabled = details.zoom_disabled();
  event->data.pinchUpdate.scale = details.scale();

  return event;
}

}  // namespace

InputMessageConverter::InputMessageConverter() {}

InputMessageConverter::~InputMessageConverter() {}

scoped_ptr<blink::WebGestureEvent> InputMessageConverter::ProcessMessage(
    const InputMessage& message) {
  scoped_ptr<blink::WebGestureEvent> event;

  switch (message.type()) {
    case InputMessage::Type_GestureScrollBegin:
      event = ProtoToGestureScrollBegin(message);
      break;
    case InputMessage::Type_GestureScrollEnd:
      event = ProtoToGestureScrollEnd(message);
      break;
    case InputMessage::Type_GestureScrollUpdate:
      event = ProtoToGestureScrollUpdate(message);
      break;
    case InputMessage::Type_GestureFlingStart:
      event = ProtoToGestureFlingStart(message);
      break;
    case InputMessage::Type_GestureFlingCancel:
      event = ProtoToGestureFlingCancel(message);
      break;
    case InputMessage::Type_GestureTap:
      event = ProtoToGestureTap(message);
      break;
    case InputMessage::Type_GesturePinchBegin:
      event = ProtoToGesturePinchBegin(message);
      break;
    case InputMessage::Type_GesturePinchEnd:
      event = ProtoToGesturePinchEnd(message);
      break;
    case InputMessage::Type_GesturePinchUpdate:
      event = ProtoToGesturePinchUpdate(message);
      break;
    case InputMessage::UNKNOWN:
      DLOG(FATAL) << "Received an InputMessage with an unknown type.";
      return nullptr;
  }

  return event;
}

}  // namespace blimp
