// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_selection_controller.h"

#include "base/logging.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

TouchSelectionController::TouchSelectionController(
    TouchSelectionControllerClient* client)
    : client_(client),
      last_input_event_type_(INPUT_EVENT_TYPE_NONE),
      start_orientation_(TOUCH_HANDLE_ORIENTATION_UNDEFINED),
      start_visible_(false),
      end_orientation_(TOUCH_HANDLE_ORIENTATION_UNDEFINED),
      end_visible_(false),
      is_insertion_active_(false),
      activate_insertion_automatically_(false),
      is_selection_active_(false),
      activate_selection_automatically_(false),
      selection_empty_(false),
      selection_editable_(false),
      temporarily_hidden_(false) {
  DCHECK(client_);
  HideAndDisallowShowingAutomatically();
}

TouchSelectionController::~TouchSelectionController() {
}

void TouchSelectionController::OnSelectionBoundsChanged(
    const gfx::RectF& start_rect,
    TouchHandleOrientation start_orientation,
    bool start_visible,
    const gfx::RectF& end_rect,
    TouchHandleOrientation end_orientation,
    bool end_visible) {
  if (!activate_selection_automatically_ && !activate_insertion_automatically_)
    return;

  if (start_rect_ == start_rect && end_rect_ == end_rect &&
      start_orientation_ == start_orientation &&
      end_orientation_ == end_orientation && start_visible_ == start_visible &&
      end_visible_ == end_visible)
    return;

  start_rect_ = start_rect;
  start_orientation_ = start_orientation;
  start_visible_ = start_visible;
  end_rect_ = end_rect;
  end_orientation_ = end_orientation;
  end_visible_ = end_visible;

  const bool is_selection_dragging =
      is_selection_active_ && (start_selection_handle_->is_dragging() ||
                               end_selection_handle_->is_dragging());

  // It's possible that the bounds temporarily overlap while a selection handle
  // is being dragged, incorrectly reporting a CENTER orientation.
  // TODO(jdduke): This safeguard is racy, as it's possible the delayed response
  // from handle positioning occurs *after* the handle dragging has ceased.
  // Instead, prevent selection -> insertion transitions without an intervening
  // action or selection clearing of some sort, crbug.com/392696.
  if (is_selection_dragging) {
    if (start_orientation_ == TOUCH_HANDLE_CENTER)
      start_orientation_ = start_selection_handle_->orientation();
    if (end_orientation_ == TOUCH_HANDLE_CENTER)
      end_orientation_ = end_selection_handle_->orientation();
  }

  const gfx::PointF start = GetStartPosition();
  const gfx::PointF end = GetEndPosition();
  if (start != end ||
      (is_selection_dragging &&
       start_orientation_ != TOUCH_HANDLE_ORIENTATION_UNDEFINED &&
       end_orientation_ != TOUCH_HANDLE_ORIENTATION_UNDEFINED)) {
    OnSelectionChanged();
    return;
  }

  if (start_orientation_ == TOUCH_HANDLE_CENTER && selection_editable_) {
    OnInsertionChanged();
    return;
  }

  HideAndDisallowShowingAutomatically();
}

bool TouchSelectionController::WillHandleTouchEvent(
    const ui::MotionEvent& event) {
  if (is_insertion_active_) {
    DCHECK(insertion_handle_);
    return insertion_handle_->WillHandleTouchEvent(event);
  }

  if (is_selection_active_) {
    DCHECK(start_selection_handle_);
    DCHECK(end_selection_handle_);
    if (start_selection_handle_->is_dragging())
      return start_selection_handle_->WillHandleTouchEvent(event);

    if (end_selection_handle_->is_dragging())
      return end_selection_handle_->WillHandleTouchEvent(event);

    const gfx::PointF event_pos(event.GetX(), event.GetY());
    if ((event_pos - GetStartPosition()).LengthSquared() <=
        (event_pos - GetEndPosition()).LengthSquared())
      return start_selection_handle_->WillHandleTouchEvent(event);
    else
      return end_selection_handle_->WillHandleTouchEvent(event);
  }

  return false;
}

void TouchSelectionController::OnLongPressEvent() {
  last_input_event_type_ = LONG_PRESS;
  ShowSelectionHandlesAutomatically();
  ShowInsertionHandleAutomatically();
  ResetCachedValues();
}

void TouchSelectionController::OnTapEvent() {
  last_input_event_type_ = TAP;
  activate_selection_automatically_ = false;
  DeactivateSelection();
  ShowInsertionHandleAutomatically();
  ResetCachedValues();
}

void TouchSelectionController::HideAndDisallowShowingAutomatically() {
  last_input_event_type_ = INPUT_EVENT_TYPE_NONE;
  DeactivateInsertion();
  DeactivateSelection();
  activate_insertion_automatically_ = false;
  activate_selection_automatically_ = false;
}

void TouchSelectionController::SetTemporarilyHidden(bool hidden) {
  if (temporarily_hidden_ == hidden)
    return;
  temporarily_hidden_ = hidden;

  TouchHandle::AnimationStyle animation_style = GetAnimationStyle(true);
  if (is_selection_active_) {
    start_selection_handle_->SetVisible(GetStartVisible(), animation_style);
    end_selection_handle_->SetVisible(GetEndVisible(), animation_style);
  }
  if (is_insertion_active_)
    insertion_handle_->SetVisible(GetStartVisible(), animation_style);
}

void TouchSelectionController::OnSelectionEditable(bool editable) {
  if (selection_editable_ == editable)
    return;
  selection_editable_ = editable;
  ResetCachedValues();
  if (!selection_editable_)
    DeactivateInsertion();
}

void TouchSelectionController::OnSelectionEmpty(bool empty) {
  if (selection_empty_ == empty)
    return;
  selection_empty_ = empty;
  ResetCachedValues();
}

bool TouchSelectionController::Animate(base::TimeTicks frame_time) {
  if (is_insertion_active_)
    return insertion_handle_->Animate(frame_time);

  if (is_selection_active_) {
    bool needs_animate = start_selection_handle_->Animate(frame_time);
    needs_animate |= end_selection_handle_->Animate(frame_time);
    return needs_animate;
  }

  return false;
}

void TouchSelectionController::OnHandleDragBegin(const TouchHandle& handle) {
  if (&handle == insertion_handle_.get())
    return;

  if (&handle == start_selection_handle_.get()) {
    fixed_handle_position_ = end_selection_handle_->position() -
                             gfx::Vector2dF(0, GetEndLineHeight() / 2.f);
  } else {
    fixed_handle_position_ = start_selection_handle_->position() -
                             gfx::Vector2dF(0, GetStartLineHeight() / 2.f);
  }
}

void TouchSelectionController::OnHandleDragUpdate(const TouchHandle& handle,
                                                  const gfx::PointF& position) {
  // As the position corresponds to the bottom left point of the selection
  // bound, offset it by half the corresponding line height.
  float half_line_height = &handle == end_selection_handle_.get()
                               ? GetEndLineHeight() / 2.f
                               : GetStartLineHeight() / 2.f;
  gfx::PointF line_position = position - gfx::Vector2dF(0, half_line_height);
  if (&handle == insertion_handle_.get()) {
    client_->MoveCaret(line_position);
  } else {
    client_->SelectBetweenCoordinates(fixed_handle_position_, line_position);
  }
}

void TouchSelectionController::OnHandleDragEnd(const TouchHandle& handle) {
}

void TouchSelectionController::OnHandleTapped(const TouchHandle& handle) {
  if (insertion_handle_ && &handle == insertion_handle_.get())
    client_->OnSelectionEvent(INSERTION_TAPPED, GetStartPosition());
}

void TouchSelectionController::SetNeedsAnimate() {
  client_->SetNeedsAnimate();
}

scoped_ptr<TouchHandleDrawable> TouchSelectionController::CreateDrawable() {
  return client_->CreateDrawable();
}

void TouchSelectionController::ShowInsertionHandleAutomatically() {
  if (activate_insertion_automatically_)
    return;
  activate_insertion_automatically_ = true;
  if (!is_insertion_active_ && !is_selection_active_)
    ResetCachedValues();
}

void TouchSelectionController::ShowSelectionHandlesAutomatically() {
  if (activate_selection_automatically_)
    return;
  activate_selection_automatically_ = true;
  if (!is_insertion_active_ && !is_selection_active_)
    ResetCachedValues();
}

void TouchSelectionController::OnInsertionChanged() {
  DeactivateSelection();

  if (last_input_event_type_ == TAP && selection_empty_) {
    HideAndDisallowShowingAutomatically();
    return;
  }

  if (!activate_insertion_automatically_)
    return;

  const bool was_active = is_insertion_active_;
  const gfx::PointF position = GetStartPosition();
  if (!is_insertion_active_)
    ActivateInsertion();
  else
    client_->OnSelectionEvent(INSERTION_MOVED, position);

  insertion_handle_->SetVisible(GetStartVisible(),
                                GetAnimationStyle(was_active));
  insertion_handle_->SetPosition(position);
}

void TouchSelectionController::OnSelectionChanged() {
  DeactivateInsertion();

  if (!activate_selection_automatically_)
    return;

  const bool was_active = is_selection_active_;
  ActivateSelection();

  const TouchHandle::AnimationStyle animation = GetAnimationStyle(was_active);
  start_selection_handle_->SetVisible(GetStartVisible(), animation);
  end_selection_handle_->SetVisible(GetEndVisible(), animation);

  start_selection_handle_->SetPosition(GetStartPosition());
  end_selection_handle_->SetPosition(GetEndPosition());
}

void TouchSelectionController::ActivateInsertion() {
  DCHECK(!is_selection_active_);

  if (!insertion_handle_)
    insertion_handle_.reset(new TouchHandle(this, TOUCH_HANDLE_CENTER));

  if (!is_insertion_active_) {
    is_insertion_active_ = true;
    insertion_handle_->SetEnabled(true);
    client_->OnSelectionEvent(INSERTION_SHOWN, GetStartPosition());
  }
}

void TouchSelectionController::DeactivateInsertion() {
  if (!is_insertion_active_)
    return;
  DCHECK(insertion_handle_);
  is_insertion_active_ = false;
  insertion_handle_->SetEnabled(false);
  client_->OnSelectionEvent(INSERTION_CLEARED, gfx::PointF());
}

void TouchSelectionController::ActivateSelection() {
  DCHECK(!is_insertion_active_);

  if (!start_selection_handle_) {
    start_selection_handle_.reset(new TouchHandle(this, start_orientation_));
  } else {
    start_selection_handle_->SetEnabled(true);
    start_selection_handle_->SetOrientation(start_orientation_);
  }

  if (!end_selection_handle_) {
    end_selection_handle_.reset(new TouchHandle(this, end_orientation_));
  } else {
    end_selection_handle_->SetEnabled(true);
    end_selection_handle_->SetOrientation(end_orientation_);
  }

  if (!is_selection_active_) {
    is_selection_active_ = true;
    client_->OnSelectionEvent(SELECTION_SHOWN, GetStartPosition());
  }
}

void TouchSelectionController::DeactivateSelection() {
  if (!is_selection_active_)
    return;
  DCHECK(start_selection_handle_);
  DCHECK(end_selection_handle_);
  start_selection_handle_->SetEnabled(false);
  end_selection_handle_->SetEnabled(false);
  is_selection_active_ = false;
  client_->OnSelectionEvent(SELECTION_CLEARED, gfx::PointF());
}

void TouchSelectionController::ResetCachedValues() {
  start_rect_ = gfx::RectF();
  end_rect_ = gfx::RectF();
  start_orientation_ = TOUCH_HANDLE_ORIENTATION_UNDEFINED;
  end_orientation_ = TOUCH_HANDLE_ORIENTATION_UNDEFINED;
  start_visible_ = false;
  end_visible_ = false;
}

gfx::PointF TouchSelectionController::GetStartPosition() const {
  return start_rect_.bottom_left();
}

gfx::PointF TouchSelectionController::GetEndPosition() const {
  return end_rect_.bottom_left();
}

float TouchSelectionController::GetStartLineHeight() const {
  return start_rect_.height();
}

float TouchSelectionController::GetEndLineHeight() const {
  return end_rect_.height();
}

bool TouchSelectionController::GetStartVisible() const {
  return start_visible_ && !temporarily_hidden_;
}

bool TouchSelectionController::GetEndVisible() const {
  return end_visible_ && !temporarily_hidden_;
}

TouchHandle::AnimationStyle TouchSelectionController::GetAnimationStyle(
    bool was_active) const {
  return was_active && client_->SupportsAnimation()
             ? TouchHandle::ANIMATION_SMOOTH
             : TouchHandle::ANIMATION_NONE;
}

}  // namespace content
