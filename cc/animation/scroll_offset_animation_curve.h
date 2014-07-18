// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLL_OFFSET_ANIMATION_CURVE_H_
#define CC_ANIMATION_SCROLL_OFFSET_ANIMATION_CURVE_H_

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "cc/animation/animation_curve.h"
#include "cc/base/cc_export.h"

namespace cc {

class TimingFunction;

class CC_EXPORT ScrollOffsetAnimationCurve : public AnimationCurve {
 public:
  static scoped_ptr<ScrollOffsetAnimationCurve> Create(
      const gfx::Vector2dF& target_value,
      scoped_ptr<TimingFunction> timing_function);

  virtual ~ScrollOffsetAnimationCurve();

  void SetInitialValue(const gfx::Vector2dF& initial_value);
  gfx::Vector2dF GetValue(double t) const;
  gfx::Vector2dF target_value() const { return target_value_; }
  void UpdateTarget(double t, const gfx::Vector2dF& new_target);

  // AnimationCurve implementation
  virtual double Duration() const OVERRIDE;
  virtual CurveType Type() const OVERRIDE;
  virtual scoped_ptr<AnimationCurve> Clone() const OVERRIDE;

 private:
  ScrollOffsetAnimationCurve(const gfx::Vector2dF& target_value,
                             scoped_ptr <TimingFunction> timing_function);

  gfx::Vector2dF initial_value_;
  gfx::Vector2dF target_value_;
  base::TimeDelta total_animation_duration_;

  // Time from animation start to most recent UpdateTarget.
  base::TimeDelta last_retarget_;

  scoped_ptr<TimingFunction> timing_function_;

  DISALLOW_COPY_AND_ASSIGN(ScrollOffsetAnimationCurve);
};

}  // namespace cc

#endif  // CC_ANIMATION_SCROLL_OFFSET_ANIMATION_CURVE_H_
