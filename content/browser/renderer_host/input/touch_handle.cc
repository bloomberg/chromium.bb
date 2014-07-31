// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_handle.h"

#include <cmath>

namespace content {

namespace {

// Maximum duration of a fade sequence.
const double kFadeDurationMs = 200;

// Maximum amount of travel for a fade sequence. This avoids handle "ghosting"
// when the handle is moving rapidly while the fade is active.
const double kFadeDistanceSquared = 20.f * 20.f;

// The maximum touch size to use when computing whether a touch point is
// targetting a touch handle. This is necessary for devices that misreport
// touch radii, preventing inappropriately largely touch sizes from completely
// breaking handle dragging behavior.
const float kMaxTouchMajorForHitTesting = 48.f;

}  // namespace

// Responsible for rendering a selection or insertion handle for text editing.
TouchHandle::TouchHandle(TouchHandleClient* client,
                         TouchHandleOrientation orientation)
    : drawable_(client->CreateDrawable()),
      client_(client),
      orientation_(orientation),
      deferred_orientation_(TOUCH_HANDLE_ORIENTATION_UNDEFINED),
      alpha_(0.f),
      animate_deferred_fade_(false),
      enabled_(true),
      is_visible_(false),
      is_dragging_(false) {
  DCHECK_NE(orientation, TOUCH_HANDLE_ORIENTATION_UNDEFINED);
  drawable_->SetEnabled(enabled_);
  drawable_->SetOrientation(orientation_);
  drawable_->SetAlpha(alpha_);
  drawable_->SetVisible(is_visible_);
  drawable_->SetFocus(position_);
}

TouchHandle::~TouchHandle() {
}

void TouchHandle::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  if (!enabled) {
    EndDrag();
    EndFade();
  }
  enabled_ = enabled;
  drawable_->SetEnabled(enabled);
}

void TouchHandle::SetVisible(bool visible, AnimationStyle animation_style) {
  DCHECK(enabled_);
  if (is_visible_ == visible)
    return;

  is_visible_ = visible;

  // Handle repositioning may have been deferred while previously invisible.
  if (visible)
    drawable_->SetFocus(position_);

  bool animate = animation_style != ANIMATION_NONE;
  if (is_dragging_) {
    animate_deferred_fade_ = animate;
    return;
  }

  if (animate)
    BeginFade();
  else
    EndFade();
}

void TouchHandle::SetPosition(const gfx::PointF& position) {
  DCHECK(enabled_);
  if (position_ == position)
    return;
  position_ = position;
  // Suppress repositioning a handle while invisible or fading out to prevent it
  // from "ghosting" outside the visible bounds. The position will be pushed to
  // the drawable when the handle regains visibility (see |SetVisible()|).
  if (is_visible_)
    drawable_->SetFocus(position_);
}

void TouchHandle::SetOrientation(TouchHandleOrientation orientation) {
  DCHECK(enabled_);
  DCHECK_NE(orientation, TOUCH_HANDLE_ORIENTATION_UNDEFINED);
  if (is_dragging_) {
    deferred_orientation_ = orientation;
    return;
  }
  DCHECK_EQ(deferred_orientation_, TOUCH_HANDLE_ORIENTATION_UNDEFINED);
  if (orientation_ == orientation)
    return;

  orientation_ = orientation;
  drawable_->SetOrientation(orientation);
}

bool TouchHandle::WillHandleTouchEvent(const ui::MotionEvent& event) {
  if (!enabled_)
    return false;

  if (!is_dragging_ && event.GetAction() != ui::MotionEvent::ACTION_DOWN)
    return false;

  switch (event.GetAction()) {
    case ui::MotionEvent::ACTION_DOWN: {
      if (!is_visible_)
        return false;
      const float touch_size =
          std::min(event.GetTouchMajor(), kMaxTouchMajorForHitTesting);
      const gfx::RectF touch_rect(event.GetX() - touch_size * .5f,
                                  event.GetY() - touch_size * .5f,
                                  touch_size,
                                  touch_size);
      if (!drawable_->IntersectsWith(touch_rect))
        return false;
      touch_down_position_ = gfx::PointF(event.GetX(), event.GetY());
      touch_to_focus_offset_ = position_ - touch_down_position_;
      touch_down_time_ = event.GetEventTime();
      BeginDrag();
    } break;

    case ui::MotionEvent::ACTION_MOVE: {
      gfx::PointF new_position =
          gfx::PointF(event.GetX(), event.GetY()) + touch_to_focus_offset_;
      client_->OnHandleDragUpdate(*this, new_position);
    } break;

    case ui::MotionEvent::ACTION_UP: {
      // TODO(jdduke): Use the platform touch slop distance and tap delay to
      // properly detect a tap, crbug.com/394093.
      base::TimeDelta delay = event.GetEventTime() - touch_down_time_;
      if (delay < base::TimeDelta::FromMilliseconds(180))
        client_->OnHandleTapped(*this);

      EndDrag();
    } break;

    case ui::MotionEvent::ACTION_CANCEL:
      EndDrag();
      break;

    default:
      break;
  };
  return true;
}

bool TouchHandle::Animate(base::TimeTicks frame_time) {
  if (fade_end_time_ == base::TimeTicks())
    return false;

  DCHECK(enabled_);

  float time_u =
      1.f - (fade_end_time_ - frame_time).InMillisecondsF() / kFadeDurationMs;
  float position_u =
      (position_ - fade_start_position_).LengthSquared() / kFadeDistanceSquared;
  float u = std::max(time_u, position_u);
  SetAlpha(is_visible_ ? u : 1.f - u);

  if (u >= 1.f) {
    EndFade();
    return false;
  }

  return true;
}

void TouchHandle::BeginDrag() {
  DCHECK(enabled_);
  if (is_dragging_)
    return;
  EndFade();
  is_dragging_ = true;
  client_->OnHandleDragBegin(*this);
}

void TouchHandle::EndDrag() {
  DCHECK(enabled_);
  if (!is_dragging_)
    return;

  is_dragging_ = false;
  client_->OnHandleDragEnd(*this);

  if (deferred_orientation_ != TOUCH_HANDLE_ORIENTATION_UNDEFINED) {
    TouchHandleOrientation deferred_orientation = deferred_orientation_;
    deferred_orientation_ = TOUCH_HANDLE_ORIENTATION_UNDEFINED;
    SetOrientation(deferred_orientation);
  }

  if (animate_deferred_fade_) {
    BeginFade();
  } else {
    // As drawable visibility assignment is deferred while dragging, push the
    // change by forcing fade completion.
    EndFade();
  }
}

void TouchHandle::BeginFade() {
  DCHECK(enabled_);
  DCHECK(!is_dragging_);
  animate_deferred_fade_ = false;
  const float target_alpha = is_visible_ ? 1.f : 0.f;
  if (target_alpha == alpha_) {
    EndFade();
    return;
  }

  drawable_->SetVisible(true);
  fade_end_time_ = base::TimeTicks::Now() +
                   base::TimeDelta::FromMillisecondsD(
                       kFadeDurationMs * std::abs(target_alpha - alpha_));
  fade_start_position_ = position_;
  client_->SetNeedsAnimate();
}

void TouchHandle::EndFade() {
  DCHECK(enabled_);
  animate_deferred_fade_ = false;
  fade_end_time_ = base::TimeTicks();
  SetAlpha(is_visible_ ? 1.f : 0.f);
  drawable_->SetVisible(is_visible_);
}

void TouchHandle::SetAlpha(float alpha) {
  alpha = std::max(0.f, std::min(1.f, alpha));
  if (alpha_ == alpha)
    return;
  alpha_ = alpha;
  drawable_->SetAlpha(alpha);
}

}  // namespace content
