// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/content_input_delegate.h"

#include "base/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/vr/platform_controller.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"

namespace vr {

namespace {

static constexpr gfx::PointF kOutOfBoundsPoint = {-0.5f, -0.5f};

}  // namespace

ContentInputDelegate::ContentInputDelegate() {}

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

void ContentInputDelegate::OnFocusChanged(bool focused) {
  // The call below tells the renderer to clear the focused element. Note that
  // we don't need to do anything when focused is true because the renderer
  // already knows about the focused element.
  if (!focused)
    content_->ClearFocusedElement();
}

void ContentInputDelegate::OnWebInputEdited(const EditedText& info,
                                            bool commit) {
  if (!content_)
    return;

  last_keyboard_edit_ = info;

  if (commit) {
    content_->SubmitWebInput();
    return;
  }

  content_->OnWebInputEdited(info.GetDiff());
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
  gesture.SetPositionInWidget(ScalePoint(normalized_content_hit_point,
                                         content_tex_css_width_,
                                         content_tex_css_height_));
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

void ContentInputDelegate::OnWebInputIndicesChanged(
    int selection_start,
    int selection_end,
    int composition_start,
    int composition_end,
    base::OnceCallback<void(const TextInputInfo&)> callback) {
  // The purpose of this method is to determine if we need to query content for
  // the text surrounding the currently focused web input field.

  // If the changed indices match with that from the last keyboard edit, then
  // this is called in response to the user entering text using the keyboard, so
  // we already know the text and don't need to ask content for it.
  TextInputInfo i = last_keyboard_edit_.current;
  if (i.selection_start == selection_start &&
      i.selection_end == selection_end &&
      i.composition_start == composition_start &&
      i.composition_end == composition_end) {
    base::ResetAndReturn(&callback).Run(i);
    return;
  }

  // If the changed indices are the same as the previous ones, this is probably
  // called as a side-effect of us requesting the text state below, so it's safe
  // to ignore this update. If this is not called as a side-effect of us
  // requesting the text state below, and the indices just happen to match the
  // previous state, it's still okay to ignore this update. Consider the
  // following scenario: 1) State after last request for web input state:
  //     This is a test|
  // 2) JS on the page changed the web input state to:
  //     This blah test|
  // In this case, 2) will trigger this function and we'll ignore it. The next
  // time user types something, we'll calculate the diff from our stale version
  // of the web input state, but because we're committing the delta between the
  // previous and the current keyboard state, the update to content will still
  // be correct. That is, even if the keyboard works with the incorrect version
  // of the text state, the end result is still correct from the user's point of
  // view. This is also how android IME works (it only requests text state when
  // the indices actually change).
  i = pending_text_input_info_;
  if (i.selection_start == selection_start &&
      i.selection_end == selection_end &&
      i.composition_start == composition_start &&
      i.composition_end == composition_end) {
    return;
  }

  // The indices changed and we need to query the update state.
  pending_text_input_info_.selection_start = selection_start;
  pending_text_input_info_.selection_end = selection_end;
  pending_text_input_info_.composition_start = composition_start;
  pending_text_input_info_.composition_end = composition_end;
  update_state_callbacks_.emplace(std::move(callback));
  content_->RequestWebInputText(base::BindOnce(
      &ContentInputDelegate::OnWebInputTextChanged, base::Unretained(this)));
}

void ContentInputDelegate::OnWebInputTextChanged(const base::string16& text) {
  pending_text_input_info_.text = text;
  DCHECK(!update_state_callbacks_.empty());

  auto update_state_callback = std::move(update_state_callbacks_.front());
  update_state_callbacks_.pop();
  base::ResetAndReturn(&update_state_callback).Run(pending_text_input_info_);
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

  auto mouse_event = std::make_unique<blink::WebMouseEvent>(
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
