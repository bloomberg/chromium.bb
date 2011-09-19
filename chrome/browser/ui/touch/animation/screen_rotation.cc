// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/animation/screen_rotation.h"

#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"
#include "views/paint_lock.h"
#include "views/view.h"
#include "views/widget/widget.h"

namespace {
const int kDefaultTransitionDurationMs = 350;

} // namespace

////////////////////////////////////////////////////////////////////////////////
// ScreenRotationListener public:
//

ScreenRotationListener::~ScreenRotationListener() {
}

////////////////////////////////////////////////////////////////////////////////
// ScreenRotation public:
//

ScreenRotation::ScreenRotation(views::View* view,
                               ScreenRotationListener* listener,
                               float old_degrees,
                               float new_degrees)
    : view_(view),
      widget_(view->GetWidget()),
      listener_(listener),
      old_degrees_(old_degrees),
      new_degrees_(new_degrees),
      duration_(kDefaultTransitionDurationMs),
      animation_started_(false),
      animation_stopped_(false) {
  DCHECK(view);
  DCHECK(listener);

  if (!view->layer() || !widget_) {
    Finalize();
  } else {
    // Screen rotations are trigged as a result of a call to SetTransform which
    // causes a paint to be scheduled to occur. At this point, the paint has
    // been scheduled, but has not yet been started. We will listen for this
    // paint to be completed before we start animating.
    view->layer()->compositor()->AddObserver(this);
  }
}

ScreenRotation::~ScreenRotation() {
  if (view_->layer())
    view_->layer()->compositor()->RemoveObserver(this);
}

void ScreenRotation::Stop() {
  animation_.reset();
  if (view_->layer()) {
    if (!interpolated_transform_.get()) {
      // attempt to initialize.
      Init();
    }
    if (interpolated_transform_.get()) {
      view_->layer()->SetTransform(interpolated_transform_->Interpolate(1.0));
      view_->GetWidget()->SchedulePaintInRect(
          view_->GetWidget()->GetClientAreaScreenBounds());
    }
  }
  Finalize();
}

////////////////////////////////////////////////////////////////////////////////
// ScreenRotation private:
//

void ScreenRotation::AnimationProgressed(const ui::Animation* anim) {
  TRACE_EVENT0("ScreenRotation", "step");
  view_->layer()->SetTransform(interpolated_transform_->Interpolate(
      anim->GetCurrentValue()));
  widget_->SchedulePaintInRect(widget_->GetClientAreaScreenBounds());
}

void ScreenRotation::AnimationEnded(const ui::Animation* anim) {
  TRACE_EVENT_END0("ScreenRotation", "ScreenRotation");
  // TODO(vollick) massage matrix so that entries sufficiently close
  // to 0, 1, or -1 are clamped to these values. The idea is to fight
  // accumulated numeric error due to successive rotations.
  ui::Transform xform = view_->layer()->transform();
  gfx::Point origin;
  xform.TransformPoint(origin);
  ui::Transform translation;
  translation.SetTranslate(new_origin_.x() - origin.x(),
                           new_origin_.y() - origin.y());
  xform.ConcatTransform(translation);
  view_->layer()->SetTransform(xform);
  widget_->SchedulePaintInRect(widget_->GetClientAreaScreenBounds());
  animation_stopped_ = true;
}

void ScreenRotation::OnCompositingEnded(ui::Compositor* compositor) {
  DoPendingWork();
}

void ScreenRotation::Init() {
  TRACE_EVENT0("ScreenRotation", "init");

  ui::Transform current_transform = view_->layer()->transform();
  int degrees = new_degrees_ - old_degrees_;
  degrees = NormalizeAngle(degrees);

  // No rotation required.
  if (degrees == 0)
    return;

  gfx::Point old_pivot;
  gfx::Point new_pivot;
  int width = view_->layer()->bounds().width();
  int height = view_->layer()->bounds().height();

  switch (degrees) {
    case 90:
      new_origin_ = new_pivot = gfx::Point(width, 0);
      new_size_.SetSize(height, width);
      break;
    case -90:
      new_origin_ = new_pivot = gfx::Point(0, height);
      new_size_.SetSize(height, width);
      break;
    case 180:
      duration_ = 550;
      new_pivot = old_pivot = gfx::Point(width / 2, height / 2);
      new_origin_.SetPoint(width, height);
      new_size_.SetSize(width, height);
      break;
  }

  // Convert points to world space.
  current_transform.TransformPoint(old_pivot);
  current_transform.TransformPoint(new_pivot);
  current_transform.TransformPoint(new_origin_);

  scoped_ptr<ui::InterpolatedTransform> rotation(
      new ui::InterpolatedTransformAboutPivot(
          old_pivot,
          new ui::InterpolatedRotation(0, degrees)));

  scoped_ptr<ui::InterpolatedTransform> translation(
      new ui::InterpolatedTranslation(
          gfx::Point(0, 0),
          gfx::Point(new_pivot.x() - old_pivot.x(),
                     new_pivot.y() - old_pivot.y())));

  float scale_factor = 0.9f;
  scoped_ptr<ui::InterpolatedTransform> scale_down(
      new ui::InterpolatedScale(1.0f, scale_factor, 0.0f, 0.5f));

  scoped_ptr<ui::InterpolatedTransform> scale_up(
      new ui::InterpolatedScale(1.0f, 1.0f / scale_factor, 0.5f, 1.0f));

  interpolated_transform_.reset(
      new ui::InterpolatedConstantTransform(current_transform));

  scale_up->SetChild(scale_down.release());
  translation->SetChild(scale_up.release());
  rotation->SetChild(translation.release());
  interpolated_transform_->SetChild(rotation.release());
}

void ScreenRotation::Start() {
  TRACE_EVENT_BEGIN0("ScreenRotation", "ScreenRotation");
  Init();
  if (interpolated_transform_.get()) {
    paint_lock_.reset(new views::PaintLock(view_));
    animation_.reset(new ui::SlideAnimation(this));
    animation_->SetTweenType(ui::Tween::LINEAR);
    animation_->SetSlideDuration(duration_);
    animation_->Show();
    animation_started_ = true;
  } else {
    Finalize();
  }
}

void ScreenRotation::Finalize() {
  ui::Transform final_transform = view_->GetTransform();
  gfx::Rect final_bounds(0, 0, new_size_.width(), new_size_.height());
  listener_->OnScreenRotationCompleted(final_transform, final_bounds);
}

int ScreenRotation::NormalizeAngle(int degrees) {
  while (degrees <= -180) degrees += 360;
  while (degrees > 180) degrees -= 360;
  return degrees;
}

void ScreenRotation::DoPendingWork() {
  if (!animation_started_)
    Start();
  else if (animation_stopped_) {
    view_->layer()->compositor()->RemoveObserver(this);
    Finalize();
  }
}
