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

class CC_EXPORT FloatKeyframe : public Keyframe {
 public:
  static scoped_ptr<FloatKeyframe> Create(
      double time,
      float value,
      scoped_ptr<TimingFunction> timing_function);
  virtual ~FloatKeyframe();

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
  virtual ~TransformKeyframe();

  const TransformOperations& Value() const;

  scoped_ptr<TransformKeyframe> Clone() const;

 private:
  TransformKeyframe(
      double time,
      const TransformOperations& value,
      scoped_ptr<TimingFunction> timing_function);

  TransformOperations value_;
};

class CC_EXPORT KeyframedFloatAnimationCurve : public FloatAnimationCurve {
 public:
  // It is required that the keyframes be sorted by time.
  static scoped_ptr<KeyframedFloatAnimationCurve> Create();

  virtual ~KeyframedFloatAnimationCurve();

  void AddKeyframe(scoped_ptr<FloatKeyframe> keyframe);

  // AnimationCurve implementation
  virtual double Duration() const OVERRIDE;
  virtual scoped_ptr<AnimationCurve> Clone() const OVERRIDE;

  // FloatAnimationCurve implementation
  virtual float GetValue(double t) const OVERRIDE;

 private:
  KeyframedFloatAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  ScopedPtrVector<FloatKeyframe> keyframes_;

  DISALLOW_COPY_AND_ASSIGN(KeyframedFloatAnimationCurve);
};

class CC_EXPORT KeyframedTransformAnimationCurve
    : public TransformAnimationCurve {
 public:
  // It is required that the keyframes be sorted by time.
  static scoped_ptr<KeyframedTransformAnimationCurve> Create();

  virtual ~KeyframedTransformAnimationCurve();

  void AddKeyframe(scoped_ptr<TransformKeyframe> keyframe);

  // AnimationCurve implementation
  virtual double Duration() const OVERRIDE;
  virtual scoped_ptr<AnimationCurve> Clone() const OVERRIDE;

  // TransformAnimationCurve implementation
  virtual gfx::Transform GetValue(double t) const OVERRIDE;

 private:
  KeyframedTransformAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  ScopedPtrVector<TransformKeyframe> keyframes_;

  DISALLOW_COPY_AND_ASSIGN(KeyframedTransformAnimationCurve);
};

}  // namespace cc

#endif  // CC_ANIMATION_KEYFRAMED_ANIMATION_CURVE_H_
