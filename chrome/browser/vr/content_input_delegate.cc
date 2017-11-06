// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/content_input_delegate.h"

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chrome/browser/vr/platform_controller.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"

namespace vr {

namespace {

static constexpr gfx::PointF kOutOfBoundsPoint = {-0.5f, -0.5f};

}  // namespace

ContentInputDelegate::ContentInputDelegate(ContentInputForwarder* content)
    : content_(content) {}

ContentInputDelegate::~ContentInputDelegate() = default;

void ContentInputDelegate::OnContentEnter(
    const gfx::PointF& normalized_hit_point) {
  SendGestureToContent(
      MakeMouseEvent(blink::WebInputEvent::kMouseEnter, normalized_hit_point));
}

void ContentInputDelegate::OnContentLeave() {
  // Note that we send an out of bounds mouse leave event. With blink feature
  // UpdateHoverPostLayout turned on, a MouseMove event will dispatched post a
  // Layout. Sending a mouse leave event at 0,0 will result continuous
  // MouseMove events sent to the content if the content keeps relayout itself.
  // See crbug.com/762573 for details.
  SendGestureToContent(
      MakeMouseEvent(blink::WebInputEvent::kMouseLeave, kOutOfBoundsPoint));
}

void ContentInputDelegate::OnContentMove(
    const gfx::PointF& normalized_hit_point) {
  SendGestureToContent(
      MakeMouseEvent(blink::WebInputEvent::kMouseMove, normalized_hit_point));
}

void ContentInputDelegate::OnContentDown(
    const gfx::PointF& normalized_hit_point) {
  SendGestureToContent(
      MakeMouseEvent(blink::WebInputEvent::kMouseDown, normalized_hit_point));
}

void ContentInputDelegate::OnContentUp(
    const gfx::PointF& normalized_hit_point) {
  SendGestureToContent(
      MakeMouseEvent(blink::WebInputEvent::kMouseUp, normalized_hit_point));
}

void ContentInputDelegate::OnContentFlingStart(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {
  UpdateGesture(normalized_hit_point, *gesture);
  SendGestureToContent(std::move(gesture));
}

void ContentInputDelegate::OnContentFlingCancel(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {
  UpdateGesture(normalized_hit_point, *gesture);
  SendGestureToContent(std::move(gesture));
}

void ContentInputDelegate::OnContentScrollBegin(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {
  UpdateGesture(normalized_hit_point, *gesture);
  SendGestureToContent(std::move(gesture));
}

void ContentInputDelegate::OnContentScrollUpdate(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {
  UpdateGesture(normalized_hit_point, *gesture);
  SendGestureToContent(std::move(gesture));
}

void ContentInputDelegate::OnContentScrollEnd(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {
  UpdateGesture(normalized_hit_point, *gesture);
  SendGestureToContent(std::move(gesture));
}

void ContentInputDelegate::OnSwapContents(int new_content_id) {
  content_id_ = new_content_id;
}

void ContentInputDelegate::UpdateGesture(
    const gfx::PointF& normalized_content_hit_point,
    blink::WebGestureEvent& gesture) {
  gesture.x = content_tex_css_width_ * normalized_content_hit_point.x();
  gesture.y = content_tex_css_height_ * normalized_content_hit_point.y();
}

void ContentInputDelegate::SendGestureToContent(
    std::unique_ptr<blink::WebInputEvent> event) {
  if (!event || !content_ || ContentGestureIsLocked(event->GetType()))
    return;

  content_->ForwardEvent(std::move(event), content_id_);
}

bool ContentInputDelegate::ContentGestureIsLocked(
    blink::WebInputEvent::Type type) {
  // TODO (asimjour) create a new MouseEnter event when we swap webcontents and
  // pointer is on the content quad.
  if (type == blink::WebInputEvent::kGestureScrollBegin ||
      type == blink::WebInputEvent::kMouseMove ||
      type == blink::WebInputEvent::kMouseDown ||
      type == blink::WebInputEvent::kMouseEnter)
    locked_content_id_ = content_id_;

  if (locked_content_id_ != content_id_)
    return true;
  return false;
}

void ContentInputDelegate::OnContentBoundsChanged(int width, int height) {
  content_tex_css_width_ = width;
  content_tex_css_height_ = height;
}

std::unique_ptr<blink::WebMouseEvent> ContentInputDelegate::MakeMouseEvent(
    blink::WebInputEvent::Type type,
    const gfx::PointF& normalized_web_content_location) {
  if (!controller_)
    return nullptr;

  gfx::Point location(
      content_tex_css_width_ * normalized_web_content_location.x(),
      content_tex_css_height_ * normalized_web_content_location.y());
  blink::WebInputEvent::Modifiers modifiers =
      controller_->IsButtonDown(PlatformController::kButtonSelect)
          ? blink::WebInputEvent::kLeftButtonDown
          : blink::WebInputEvent::kNoModifiers;

  base::TimeTicks timestamp;
  switch (type) {
    case blink::WebInputEvent::kMouseUp:
    case blink::WebInputEvent::kMouseDown:
      timestamp = controller_->GetLastButtonTimestamp();
      break;
    case blink::WebInputEvent::kMouseMove:
    case blink::WebInputEvent::kMouseEnter:
    case blink::WebInputEvent::kMouseLeave:
      timestamp = controller_->GetLastOrientationTimestamp();
      break;
    default:
      NOTREACHED();
  }

  auto mouse_event = base::MakeUnique<blink::WebMouseEvent>(
      type, modifiers, (timestamp - base::TimeTicks()).InSecondsF());
  mouse_event->pointer_type = blink::WebPointerProperties::PointerType::kMouse;
  mouse_event->button = blink::WebPointerProperties::Button::kLeft;
  mouse_event->SetPositionInWidget(location.x(), location.y());
  // TODO(mthiesse): Should we support double-clicks for input? What should the
  // timeout be?
  mouse_event->click_count = 1;

  return mouse_event;
}

}  // namespace vr
