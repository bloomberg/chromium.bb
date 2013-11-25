// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animation_curve.h"

#include "base/logging.h"
#include "cc/animation/timing_function.h"
#include "ui/gfx/animation/tween.h"

namespace cc {

scoped_ptr<ScrollOffsetAnimationCurve> ScrollOffsetAnimationCurve::Create(
    gfx::Vector2dF target_value,
    scoped_ptr<TimingFunction> timing_function) {
  return make_scoped_ptr(
      new ScrollOffsetAnimationCurve(target_value, timing_function.Pass()));
}

ScrollOffsetAnimationCurve::ScrollOffsetAnimationCurve(
    gfx::Vector2dF target_value,
    scoped_ptr<TimingFunction> timing_function)
    : target_value_(target_value),
      duration_(0.0),
      timing_function_(timing_function.Pass()) {}

ScrollOffsetAnimationCurve::~ScrollOffsetAnimationCurve() {}

gfx::Vector2dF ScrollOffsetAnimationCurve::GetValue(double t) const {
  if (t <= 0)
    return initial_value_;

  if (t >= duration_)
    return target_value_;

  double progress = timing_function_->GetValue(t / duration_);
  return gfx::Vector2dF(gfx::Tween::FloatValueBetween(
                            progress, initial_value_.x(), target_value_.x()),
                        gfx::Tween::FloatValueBetween(
                            progress, initial_value_.y(), target_value_.y()));
}

double ScrollOffsetAnimationCurve::Duration() const {
  return duration_;
}

AnimationCurve::CurveType ScrollOffsetAnimationCurve::Type() const {
  return ScrollOffset;
}

scoped_ptr<AnimationCurve> ScrollOffsetAnimationCurve::Clone() const {
  scoped_ptr<TimingFunction> timing_function(
      static_cast<TimingFunction*>(timing_function_->Clone().release()));
  scoped_ptr<ScrollOffsetAnimationCurve> curve_clone =
      Create(target_value_, timing_function.Pass());
  curve_clone->set_initial_value(initial_value_);
  curve_clone->set_duration(duration_);
  return curve_clone.PassAs<AnimationCurve>();
}

}  // namespace cc
