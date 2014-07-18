// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animation_curve.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "cc/animation/timing_function.h"
#include "ui/gfx/animation/tween.h"

const double kDurationDivisor = 60.0;

namespace cc {

namespace {

static float MaximumDimension(gfx::Vector2dF delta) {
  return std::max(std::abs(delta.x()), std::abs(delta.y()));
}

static base::TimeDelta DurationFromDelta(gfx::Vector2dF delta) {
  // The duration of a scroll animation depends on the size of the scroll.
  // The exact relationship between the size and the duration isn't specified
  // by the CSSOM View smooth scroll spec and is instead left up to user agents
  // to decide. The calculation performed here will very likely be further
  // tweaked before the smooth scroll API ships.
  return base::TimeDelta::FromMicroseconds(
      (std::sqrt(MaximumDimension(delta)) / kDurationDivisor) *
      base::Time::kMicrosecondsPerSecond);
}

static scoped_ptr<TimingFunction> EaseOutWithInitialVelocity(double velocity) {
  // Based on EaseInOutTimingFunction::Create with first control point rotated.
  const double r2 = 0.42 * 0.42;
  const double v2 = velocity * velocity;
  const double x1 = std::sqrt(r2 / (v2 + 1));
  const double y1 = std::sqrt(r2 * v2 / (v2 + 1));
  return CubicBezierTimingFunction::Create(x1, y1, 0.58, 1)
      .PassAs<TimingFunction>();
}

}  // namespace

scoped_ptr<ScrollOffsetAnimationCurve> ScrollOffsetAnimationCurve::Create(
    const gfx::Vector2dF& target_value,
    scoped_ptr<TimingFunction> timing_function) {
  return make_scoped_ptr(
      new ScrollOffsetAnimationCurve(target_value, timing_function.Pass()));
}

ScrollOffsetAnimationCurve::ScrollOffsetAnimationCurve(
    const gfx::Vector2dF& target_value,
    scoped_ptr<TimingFunction> timing_function)
    : target_value_(target_value), timing_function_(timing_function.Pass()) {
}

ScrollOffsetAnimationCurve::~ScrollOffsetAnimationCurve() {}

void ScrollOffsetAnimationCurve::SetInitialValue(
    const gfx::Vector2dF& initial_value) {
  initial_value_ = initial_value;
  total_animation_duration_ = DurationFromDelta(target_value_ - initial_value_);
}

gfx::Vector2dF ScrollOffsetAnimationCurve::GetValue(double t) const {
  double duration = (total_animation_duration_ - last_retarget_).InSecondsF();
  t -= last_retarget_.InSecondsF();

  if (t <= 0)
    return initial_value_;

  if (t >= duration)
    return target_value_;

  double progress = (timing_function_->GetValue(t / duration));
  return gfx::Vector2dF(gfx::Tween::FloatValueBetween(
                            progress, initial_value_.x(), target_value_.x()),
                        gfx::Tween::FloatValueBetween(
                            progress, initial_value_.y(), target_value_.y()));
}

double ScrollOffsetAnimationCurve::Duration() const {
  return total_animation_duration_.InSecondsF();
}

AnimationCurve::CurveType ScrollOffsetAnimationCurve::Type() const {
  return ScrollOffset;
}

scoped_ptr<AnimationCurve> ScrollOffsetAnimationCurve::Clone() const {
  scoped_ptr<TimingFunction> timing_function(
      static_cast<TimingFunction*>(timing_function_->Clone().release()));
  scoped_ptr<ScrollOffsetAnimationCurve> curve_clone =
      Create(target_value_, timing_function.Pass());
  curve_clone->initial_value_ = initial_value_;
  curve_clone->total_animation_duration_ = total_animation_duration_;
  curve_clone->last_retarget_ = last_retarget_;
  return curve_clone.PassAs<AnimationCurve>();
}

void ScrollOffsetAnimationCurve::UpdateTarget(
    double t,
    const gfx::Vector2dF& new_target) {
  gfx::Vector2dF current_position = GetValue(t);
  gfx::Vector2dF old_delta = target_value_ - initial_value_;
  gfx::Vector2dF new_delta = new_target - current_position;

  double old_duration =
      (total_animation_duration_ - last_retarget_).InSecondsF();
  double new_duration = DurationFromDelta(new_delta).InSecondsF();

  double old_velocity = timing_function_->Velocity(
      (t - last_retarget_.InSecondsF()) / old_duration);

  // TimingFunction::Velocity gives the slope of the curve from 0 to 1.
  // To match the "true" velocity in px/sec we must adjust this slope for
  // differences in duration and scroll delta between old and new curves.
  double new_velocity =
      old_velocity * (new_duration / old_duration) *
      (MaximumDimension(old_delta) / MaximumDimension(new_delta));

  initial_value_ = current_position;
  target_value_ = new_target;
  total_animation_duration_ = base::TimeDelta::FromSecondsD(t + new_duration);
  last_retarget_ = base::TimeDelta::FromSecondsD(t);
  timing_function_ = EaseOutWithInitialVelocity(new_velocity);
}

}  // namespace cc
