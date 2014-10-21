// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_KEYFRAMED_ANIMATION_CURVE_H_
#define CC_ANIMATION_KEYFRAMED_ANIMATION_CURVE_H_

#include "cc/animation/animation_curve.h"
#include "cc/animation/timing_function.h"
#include "cc/animation/transform_operations.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"

namespace cc {

class CC_EXPORT Keyframe {
 public:
  double Time() const;
  const TimingFunction* timing_function() const {
    return timing_function_.get();
  }

 protected:
  Keyframe(double time, scoped_ptr<TimingFunction> timing_function);
  virtual ~Keyframe();

 private:
  double time_;
  scoped_ptr<TimingFunction> timing_function_;

  DISALLOW_COPY_AND_ASSIGN(Keyframe);
};

class CC_EXPORT ColorKeyframe : public Keyframe {
 public:
  static scoped_ptr<ColorKeyframe> Create(
      double time,
      SkColor value,
      scoped_ptr<TimingFunction> timing_function);
  ~ColorKeyframe() override;

  SkColor Value() const;

  scoped_ptr<ColorKeyframe> Clone() const;

 private:
  ColorKeyframe(double time,
                SkColor value,
                scoped_ptr<TimingFunction> timing_function);

  SkColor value_;
};

class CC_EXPORT FloatKeyframe : public Keyframe {
 public:
  static scoped_ptr<FloatKeyframe> Create(
      double time,
      float value,
      scoped_ptr<TimingFunction> timing_function);
  ~FloatKeyframe() override;

  float Value() const;

  scoped_ptr<FloatKeyframe> Clone() const;

 private:
  FloatKeyframe(double time,
                float value,
                scoped_ptr<TimingFunction> timing_function);

  float value_;
};

class CC_EXPORT TransformKeyframe : public Keyframe {
 public:
  static scoped_ptr<TransformKeyframe> Create(
      double time,
      const TransformOperations& value,
      scoped_ptr<TimingFunction> timing_function);
  ~TransformKeyframe() override;

  const TransformOperations& Value() const;

  scoped_ptr<TransformKeyframe> Clone() const;

 private:
  TransformKeyframe(
      double time,
      const TransformOperations& value,
      scoped_ptr<TimingFunction> timing_function);

  TransformOperations value_;
};

class CC_EXPORT FilterKeyframe : public Keyframe {
 public:
  static scoped_ptr<FilterKeyframe> Create(
      double time,
      const FilterOperations& value,
      scoped_ptr<TimingFunction> timing_function);
  ~FilterKeyframe() override;

  const FilterOperations& Value() const;

  scoped_ptr<FilterKeyframe> Clone() const;

 private:
  FilterKeyframe(
      double time,
      const FilterOperations& value,
      scoped_ptr<TimingFunction> timing_function);

  FilterOperations value_;
};

class CC_EXPORT KeyframedColorAnimationCurve : public ColorAnimationCurve {
 public:
  // It is required that the keyframes be sorted by time.
  static scoped_ptr<KeyframedColorAnimationCurve> Create();

  ~KeyframedColorAnimationCurve() override;

  void AddKeyframe(scoped_ptr<ColorKeyframe> keyframe);
  void SetTimingFunction(scoped_ptr<TimingFunction> timing_function) {
    timing_function_ = timing_function.Pass();
  }

  // AnimationCurve implementation
  double Duration() const override;
  scoped_ptr<AnimationCurve> Clone() const override;

  // BackgrounColorAnimationCurve implementation
  SkColor GetValue(double t) const override;

 private:
  KeyframedColorAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  ScopedPtrVector<ColorKeyframe> keyframes_;
  scoped_ptr<TimingFunction> timing_function_;

  DISALLOW_COPY_AND_ASSIGN(KeyframedColorAnimationCurve);
};

class CC_EXPORT KeyframedFloatAnimationCurve : public FloatAnimationCurve {
 public:
  // It is required that the keyframes be sorted by time.
  static scoped_ptr<KeyframedFloatAnimationCurve> Create();

  ~KeyframedFloatAnimationCurve() override;

  void AddKeyframe(scoped_ptr<FloatKeyframe> keyframe);
  void SetTimingFunction(scoped_ptr<TimingFunction> timing_function) {
    timing_function_ = timing_function.Pass();
  }

  // AnimationCurve implementation
  double Duration() const override;
  scoped_ptr<AnimationCurve> Clone() const override;

  // FloatAnimationCurve implementation
  float GetValue(double t) const override;

 private:
  KeyframedFloatAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  ScopedPtrVector<FloatKeyframe> keyframes_;
  scoped_ptr<TimingFunction> timing_function_;

  DISALLOW_COPY_AND_ASSIGN(KeyframedFloatAnimationCurve);
};

class CC_EXPORT KeyframedTransformAnimationCurve
    : public TransformAnimationCurve {
 public:
  // It is required that the keyframes be sorted by time.
  static scoped_ptr<KeyframedTransformAnimationCurve> Create();

  ~KeyframedTransformAnimationCurve() override;

  void AddKeyframe(scoped_ptr<TransformKeyframe> keyframe);
  void SetTimingFunction(scoped_ptr<TimingFunction> timing_function) {
    timing_function_ = timing_function.Pass();
  }

  // AnimationCurve implementation
  double Duration() const override;
  scoped_ptr<AnimationCurve> Clone() const override;

  // TransformAnimationCurve implementation
  gfx::Transform GetValue(double t) const override;
  bool AnimatedBoundsForBox(const gfx::BoxF& box,
                            gfx::BoxF* bounds) const override;
  bool AffectsScale() const override;
  bool IsTranslation() const override;
  bool MaximumTargetScale(bool forward_direction,
                          float* max_scale) const override;

 private:
  KeyframedTransformAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  ScopedPtrVector<TransformKeyframe> keyframes_;
  scoped_ptr<TimingFunction> timing_function_;

  DISALLOW_COPY_AND_ASSIGN(KeyframedTransformAnimationCurve);
};

class CC_EXPORT KeyframedFilterAnimationCurve
    : public FilterAnimationCurve {
 public:
  // It is required that the keyframes be sorted by time.
  static scoped_ptr<KeyframedFilterAnimationCurve> Create();

  ~KeyframedFilterAnimationCurve() override;

  void AddKeyframe(scoped_ptr<FilterKeyframe> keyframe);
  void SetTimingFunction(scoped_ptr<TimingFunction> timing_function) {
    timing_function_ = timing_function.Pass();
  }

  // AnimationCurve implementation
  double Duration() const override;
  scoped_ptr<AnimationCurve> Clone() const override;

  // FilterAnimationCurve implementation
  FilterOperations GetValue(double t) const override;
  bool HasFilterThatMovesPixels() const override;

 private:
  KeyframedFilterAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  ScopedPtrVector<FilterKeyframe> keyframes_;
  scoped_ptr<TimingFunction> timing_function_;

  DISALLOW_COPY_AND_ASSIGN(KeyframedFilterAnimationCurve);
};

}  // namespace cc

#endif  // CC_ANIMATION_KEYFRAMED_ANIMATION_CURVE_H_
