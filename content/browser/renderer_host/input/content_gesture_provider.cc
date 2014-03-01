// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/content_gesture_provider.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/gesture_detection/gesture_config_helper.h"
#include "ui/events/gesture_detection/motion_event.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace content {
namespace {

WebGestureEvent CreateGesture(const ui::GestureEventData& data,
                              float scale) {
  WebGestureEvent gesture;
  gesture.x = data.x * scale;
  gesture.y = data.y * scale;
  gesture.timeStampSeconds = (data.time - base::TimeTicks()).InSecondsF();
  gesture.sourceDevice = WebGestureEvent::Touchscreen;

  switch (data.type) {
    case ui::GESTURE_SHOW_PRESS:
      gesture.type = WebInputEvent::GestureShowPress;
      gesture.data.showPress.width = data.details.show_press.width * scale;
      gesture.data.showPress.width = data.details.show_press.width * scale;
      break;
    case ui::GESTURE_DOUBLE_TAP:
      gesture.type = WebInputEvent::GestureDoubleTap;
      break;
    case ui::GESTURE_TAP:
      gesture.type = WebInputEvent::GestureTap;
      gesture.data.tap.tapCount = data.details.tap.tap_count;
      gesture.data.tap.width = data.details.tap.width * scale;
      gesture.data.tap.height = data.details.tap.height * scale;
      break;
    case ui::GESTURE_TAP_UNCONFIRMED:
      gesture.type = WebInputEvent::GestureTapUnconfirmed;
      gesture.data.tap.tapCount = data.details.tap.tap_count;
      gesture.data.tap.width = data.details.tap.width * scale;
      gesture.data.tap.height = data.details.tap.height * scale;
      break;
    case ui::GESTURE_LONG_PRESS:
      gesture.type = WebInputEvent::GestureLongPress;
      gesture.data.longPress.width = data.details.long_press.width * scale;
      gesture.data.longPress.height = data.details.long_press.height * scale;
      break;
    case ui::GESTURE_LONG_TAP:
      gesture.type = WebInputEvent::GestureLongTap;
      gesture.data.longPress.width = data.details.long_press.width * scale;
      gesture.data.longPress.height = data.details.long_press.height * scale;
      break;
    case ui::GESTURE_SCROLL_BEGIN:
      gesture.type = WebInputEvent::GestureScrollBegin;
      gesture.data.scrollBegin.deltaXHint =
          data.details.scroll_begin.delta_x_hint * scale;
      gesture.data.scrollBegin.deltaYHint =
          data.details.scroll_begin.delta_y_hint * scale;
      break;
    case ui::GESTURE_SCROLL_UPDATE:
      gesture.type = WebInputEvent::GestureScrollUpdate;
      gesture.data.scrollUpdate.deltaX =
          data.details.scroll_update.delta_x * scale;
      gesture.data.scrollUpdate.deltaY =
          data.details.scroll_update.delta_y * scale;
      gesture.data.scrollUpdate.velocityX =
          data.details.scroll_update.velocity_x * scale;
      gesture.data.scrollUpdate.velocityY =
          data.details.scroll_update.velocity_y * scale;
      break;
    case ui::GESTURE_SCROLL_END:
      gesture.type = WebInputEvent::GestureScrollEnd;
      break;
    case ui::GESTURE_FLING_START:
      gesture.type = WebInputEvent::GestureFlingStart;
      gesture.data.flingStart.velocityX =
          data.details.fling_start.velocity_x * scale;
      gesture.data.flingStart.velocityY =
          data.details.fling_start.velocity_y * scale;
      break;
    case ui::GESTURE_FLING_CANCEL:
      gesture.type = WebInputEvent::GestureFlingCancel;
      break;
    case ui::GESTURE_PINCH_BEGIN:
      gesture.type = WebInputEvent::GesturePinchBegin;
      break;
    case ui::GESTURE_PINCH_UPDATE:
      gesture.type = WebInputEvent::GesturePinchUpdate;
      gesture.data.pinchUpdate.scale = data.details.pinch_update.scale;
      break;
    case ui::GESTURE_PINCH_END:
      gesture.type = WebInputEvent::GesturePinchEnd;
      break;
    case ui::GESTURE_TAP_CANCEL:
      gesture.type = WebInputEvent::GestureTapCancel;
      break;
    case ui::GESTURE_TAP_DOWN:
      gesture.type = WebInputEvent::GestureTapDown;
      gesture.data.tapDown.width = data.details.tap_down.width * scale;
      gesture.data.tapDown.height = data.details.tap_down.height * scale;
      break;
    case ui::GESTURE_TYPE_INVALID:
      NOTREACHED() << "Invalid ui::GestureEventType provided.";
      break;
  }

  return gesture;
}

ui::TouchDispositionGestureFilter::TouchEventAck
ToTouchDispositionGestureFilterAck(InputEventAckState ack_state) {
  switch (ack_state) {
    case INPUT_EVENT_ACK_STATE_CONSUMED:
      return ui::TouchDispositionGestureFilter::CONSUMED;
    case INPUT_EVENT_ACK_STATE_NOT_CONSUMED:
      return ui::TouchDispositionGestureFilter::NOT_CONSUMED;
    case INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS:
      return ui::TouchDispositionGestureFilter::NO_CONSUMER_EXISTS;
    case INPUT_EVENT_ACK_STATE_IGNORED:
      return ui::TouchDispositionGestureFilter::CONSUMED;
    case INPUT_EVENT_ACK_STATE_UNKNOWN:
      break;
  };
  NOTREACHED() << "Invalid ack state: " << ack_state;
  return ui::TouchDispositionGestureFilter::NO_CONSUMER_EXISTS;
}

ui::GestureProvider::Config GetGestureProviderConfig() {
  ui::GestureProvider::Config config = ui::DefaultGestureProviderConfig();
#if defined(OS_ANDROID)
  config.disable_click_delay =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableClickDelay);
#endif
  return config;
}

}  // namespace

ContentGestureProvider::ContentGestureProvider(
    ContentGestureProviderClient* client,
    float touch_to_gesture_scale)
    : client_(client),
      touch_to_gesture_scale_(touch_to_gesture_scale),
      gesture_provider_(GetGestureProviderConfig(), this),
      gesture_filter_(this),
      handling_event_(false) {}

bool ContentGestureProvider::OnTouchEvent(const ui::MotionEvent& event) {
  DCHECK(!handling_event_);
  base::AutoReset<bool> handling_event(&handling_event_, true);

  pending_gesture_packet_ = ui::GestureEventDataPacket::FromTouch(event);

  if (!gesture_provider_.OnTouchEvent(event))
    return false;

  ui::TouchDispositionGestureFilter::PacketResult result =
      gesture_filter_.OnGesturePacket(pending_gesture_packet_);
  if (result != ui::TouchDispositionGestureFilter::SUCCESS) {
    NOTREACHED() << "Invalid touch gesture sequence detected.";
    return false;
  }

  return true;
}

void ContentGestureProvider::OnTouchEventAck(InputEventAckState ack_state) {
  DCHECK_NE(INPUT_EVENT_ACK_STATE_UNKNOWN, ack_state);
  gesture_filter_.OnTouchEventAck(
      ToTouchDispositionGestureFilterAck(ack_state));
}

void ContentGestureProvider::ResetGestureDetectors() {
  gesture_provider_.ResetGestureDetectors();
}

void ContentGestureProvider::CancelActiveTouchSequence() {
  gesture_provider_.CancelActiveTouchSequence();
}

void ContentGestureProvider::SetMultiTouchSupportEnabled(bool enabled) {
  gesture_provider_.SetMultiTouchSupportEnabled(enabled);
}

void ContentGestureProvider::SetDoubleTapSupportForPlatformEnabled(
    bool enabled) {
  gesture_provider_.SetDoubleTapSupportForPlatformEnabled(enabled);
}

void ContentGestureProvider::SetDoubleTapSupportForPageEnabled(bool enabled) {
  gesture_provider_.SetDoubleTapSupportForPageEnabled(enabled);
}

void ContentGestureProvider::OnGestureEvent(
    const ui::GestureEventData& event) {
  if (handling_event_) {
    pending_gesture_packet_.Push(event);
    return;
  }

  gesture_filter_.OnGesturePacket(
      ui::GestureEventDataPacket::FromTouchTimeout(event));
}

void ContentGestureProvider::ForwardGestureEvent(
    const ui::GestureEventData& event) {
  client_->OnGestureEvent(CreateGesture(event, touch_to_gesture_scale_));
}

}  // namespace content
