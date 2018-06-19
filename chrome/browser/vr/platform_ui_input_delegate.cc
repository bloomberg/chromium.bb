// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/platform_ui_input_delegate.h"

#include "base/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/vr/platform_controller.h"
#include "chrome/browser/vr/platform_input_handler.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/public/platform/web_touch_event.h"

namespace vr {

namespace {

static constexpr gfx::PointF kOutOfBoundsPoint = {-0.5f, -0.5f};

}  // namespace

PlatformUiInputDelegate::PlatformUiInputDelegate() {}

PlatformUiInputDelegate::PlatformUiInputDelegate(
    PlatformInputHandler* input_handler)
    : input_handler_(input_handler) {}

PlatformUiInputDelegate::~PlatformUiInputDelegate() = default;

void PlatformUiInputDelegate::OnHoverEnter(
    const gfx::PointF& normalized_hit_point) {
  SendGestureToTarget(
      MakeMouseEvent(blink::WebInputEvent::kMouseEnter, normalized_hit_point));
}

void PlatformUiInputDelegate::OnHoverLeave() {
  // Note that we send an out of bounds mouse leave event. With blink feature
  // UpdateHoverPostLayout turned on, a MouseMove event will dispatched post a
  // Layout. Sending a mouse leave event at 0,0 will result continuous
  // MouseMove events sent to the content if the content keeps relayout itself.
  // See https://crbug.com/762573 for details.
  SendGestureToTarget(
      MakeMouseEvent(blink::WebInputEvent::kMouseLeave, kOutOfBoundsPoint));
}

void PlatformUiInputDelegate::OnHoverMove(
    const gfx::PointF& normalized_hit_point) {
  SendGestureToTarget(
      MakeMouseEvent(blink::WebInputEvent::kMouseMove, normalized_hit_point));
}

void PlatformUiInputDelegate::OnButtonDown(
    const gfx::PointF& normalized_hit_point) {
  SendGestureToTarget(
      MakeTouchEvent(blink::WebInputEvent::kTouchStart, normalized_hit_point));
}

void PlatformUiInputDelegate::OnButtonUp(
    const gfx::PointF& normalized_hit_point) {
  SendGestureToTarget(
      MakeTouchEvent(blink::WebInputEvent::kTouchEnd, normalized_hit_point));
}

void PlatformUiInputDelegate::OnTouchMove(
    const gfx::PointF& normalized_hit_point) {
  SendGestureToTarget(
      MakeTouchEvent(blink::WebInputEvent::kTouchMove, normalized_hit_point));
}

void PlatformUiInputDelegate::OnFlingCancel(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {
  UpdateGesture(normalized_hit_point, *gesture);
  SendGestureToTarget(std::move(gesture));
}

void PlatformUiInputDelegate::OnScrollBegin(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {
  UpdateGesture(normalized_hit_point, *gesture);
  SendGestureToTarget(std::move(gesture));
}

void PlatformUiInputDelegate::OnScrollUpdate(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {
  UpdateGesture(normalized_hit_point, *gesture);
  SendGestureToTarget(std::move(gesture));
}

void PlatformUiInputDelegate::OnScrollEnd(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {
  UpdateGesture(normalized_hit_point, *gesture);
  SendGestureToTarget(std::move(gesture));
}

void PlatformUiInputDelegate::UpdateGesture(
    const gfx::PointF& normalized_content_hit_point,
    blink::WebGestureEvent& gesture) {
  gesture.SetPositionInWidget(
      ScalePoint(normalized_content_hit_point, size_.width(), size_.height()));
}

void PlatformUiInputDelegate::SendGestureToTarget(
    std::unique_ptr<blink::WebInputEvent> event) {
  if (!event || !input_handler_)
    return;

  input_handler_->ForwardEventToPlatformUi(std::move(event));
}

std::unique_ptr<blink::WebMouseEvent> PlatformUiInputDelegate::MakeMouseEvent(
    blink::WebInputEvent::Type type,
    const gfx::PointF& normalized_web_content_location) const {
  // TODO(acondor): Remove dependency on platform controller.
  if (!controller_)
    return nullptr;

  gfx::Point location = CalculateLocation(normalized_web_content_location);
  blink::WebInputEvent::Modifiers modifiers =
      controller_->IsButtonDown(PlatformController::kButtonSelect)
          ? blink::WebInputEvent::kLeftButtonDown
          : blink::WebInputEvent::kNoModifiers;

  base::TimeTicks timestamp;
  switch (type) {
    case blink::WebInputEvent::kMouseMove:
    case blink::WebInputEvent::kMouseEnter:
    case blink::WebInputEvent::kMouseLeave:
      timestamp = controller_->GetLastOrientationTimestamp();
      break;
    default:
      NOTREACHED();
  }

  auto mouse_event =
      std::make_unique<blink::WebMouseEvent>(type, modifiers, timestamp);
  mouse_event->pointer_type = blink::WebPointerProperties::PointerType::kMouse;
  mouse_event->button = blink::WebPointerProperties::Button::kLeft;
  mouse_event->SetPositionInWidget(location.x(), location.y());
  mouse_event->click_count = 1;
  return mouse_event;
}

std::unique_ptr<blink::WebTouchEvent> PlatformUiInputDelegate::MakeTouchEvent(
    blink::WebInputEvent::Type type,
    const gfx::PointF& normalized_web_content_location) const {
  // TODO(acondor): Remove dependency on platform controller.
  if (!controller_)
    return nullptr;

  gfx::Point location = CalculateLocation(normalized_web_content_location);
  blink::WebInputEvent::Modifiers modifiers =
      blink::WebInputEvent::kNoModifiers;

  base::TimeTicks timestamp;
  blink::WebTouchPoint::State touch_state =
      blink::WebTouchPoint::kStateUndefined;
  switch (type) {
    case blink::WebInputEvent::kTouchStart:
      touch_state = blink::WebTouchPoint::kStatePressed;
      timestamp = controller_->GetLastButtonTimestamp();
      break;
    case blink::WebInputEvent::kTouchEnd:
      touch_state = blink::WebTouchPoint::kStateReleased;
      timestamp = controller_->GetLastButtonTimestamp();
      break;
    case blink::WebInputEvent::kTouchMove:
      touch_state = blink::WebTouchPoint::kStateMoved;
      timestamp = controller_->GetLastOrientationTimestamp();
      break;
    default:
      NOTREACHED();
  }

  auto touch_event =
      std::make_unique<blink::WebTouchEvent>(type, modifiers, timestamp);
  touch_event->touches_length = 1;
  touch_event->touches[0].state = touch_state;
  touch_event->touches[0].SetPositionInWidget(location.x(), location.y());
  return touch_event;
}

gfx::Point PlatformUiInputDelegate::CalculateLocation(
    const gfx::PointF& normalized_web_content_location) const {
  return gfx::Point(size_.width() * normalized_web_content_location.x(),
                    size_.height() * normalized_web_content_location.y());
}

}  // namespace vr
