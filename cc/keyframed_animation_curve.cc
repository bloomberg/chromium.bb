// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/keyframed_animation_curve.h"

namespace cc {

namespace {

template <class Keyframe>
void InsertKeyframe(scoped_ptr<Keyframe> keyframe,
                    ScopedPtrVector<Keyframe>& keyframes) {
  // Usually, the keyframes will be added in order, so this loop would be
  // unnecessary and we should skip it if possible.
  if (!keyframes.empty() && keyframe->Time() < keyframes.back()->Time()) {
    for (size_t i = 0; i < keyframes.size(); ++i) {
      if (keyframe->Time() < keyframes[i]->Time()) {
        keyframes.insert(keyframes.begin() + i, keyframe.Pass());
        return;
      }
    }
  }

  keyframes.push_back(keyframe.Pass());
}

scoped_ptr<TimingFunction> CloneTimingFunction(
    const TimingFunction* timing_function) {
  DCHECK(timing_function);
  scoped_ptr<AnimationCurve> curve(timing_function->Clone());
  return scoped_ptr<TimingFunction>(
      static_cast<TimingFunction*>(curve.release()));
}

}  // namespace

Keyframe::Keyframe(double time, scoped_ptr<TimingFunction> timing_function)
    : time_(time),
      timing_function_(timing_function.Pass()) {}

Keyframe::~Keyframe() {}

double Keyframe::Time() const {
  return time_;
}

scoped_ptr<FloatKeyframe> FloatKeyframe::Create(
    double time,
    float value,
    scoped_ptr<TimingFunction> timing_function) {
  return make_scoped_ptr(
      new FloatKeyframe(time, value, timing_function.Pass()));
}

FloatKeyframe::FloatKeyframe(double time,
                             float value,
                             scoped_ptr<TimingFunction> timing_function)
    : Keyframe(time, timing_function.Pass()),
      value_(value) {}

FloatKeyframe::~FloatKeyframe() {}

float FloatKeyframe::Value() const {
  return value_;
}

scoped_ptr<FloatKeyframe> FloatKeyframe::Clone() const {
  scoped_ptr<TimingFunction> func;
  if (timing_function())
    func = CloneTimingFunction(timing_function());
  return FloatKeyframe::Create(Time(), Value(), func.Pass());
}

scoped_ptr<TransformKeyframe> TransformKeyframe::Create(
    double time,
    const TransformOperations& value,
    scoped_ptr<TimingFunction> timing_function) {
  return make_scoped_ptr(
      new TransformKeyframe(time, value, timing_function.Pass()));
}

TransformKeyframe::TransformKeyframe(double time,
                                     const TransformOperations& value,
                                     scoped_ptr<TimingFunction> timing_function)
    : Keyframe(time, timing_function.Pass()),
      value_(value) {}

TransformKeyframe::~TransformKeyframe() {}

const TransformOperations& TransformKeyframe::Value() const {
  return value_;
}

scoped_ptr<TransformKeyframe> TransformKeyframe::Clone() const {
  scoped_ptr<TimingFunction> func;
  if (timing_function())
    func = CloneTimingFunction(timing_function());
  return TransformKeyframe::Create(Time(), Value(), func.Pass());
}

scoped_ptr<KeyframedFloatAnimationCurve> KeyframedFloatAnimationCurve::
    Create() {
  return make_scoped_ptr(new KeyframedFloatAnimationCurve);
}

KeyframedFloatAnimationCurve::KeyframedFloatAnimationCurve() {}

KeyframedFloatAnimationCurve::~KeyframedFloatAnimationCurve() {}

void KeyframedFloatAnimationCurve::AddKeyframe(
    scoped_ptr<FloatKeyframe> keyframe) {
  InsertKeyframe(keyframe.Pass(), keyframes_);
}

double KeyframedFloatAnimationCurve::Duration() const {
  return keyframes_.back()->Time() - keyframes_.front()->Time();
}

scoped_ptr<AnimationCurve> KeyframedFloatAnimationCurve::Clone() const {
  scoped_ptr<KeyframedFloatAnimationCurve> to_return(
      KeyframedFloatAnimationCurve::Create());
  for (size_t i = 0; i < keyframes_.size(); ++i)
    to_return->AddKeyframe(keyframes_[i]->Clone());
  return to_return.PassAs<AnimationCurve>();
}

float KeyframedFloatAnimationCurve::GetValue(double t) const {
  if (t <= keyframes_.front()->Time())
    return keyframes_.front()->Value();

  if (t >= keyframes_.back()->Time())
    return keyframes_.back()->Value();

  size_t i = 0;
  for (; i < keyframes_.size() - 1; ++i) {
    if (t < keyframes_[i+1]->Time())
      break;
  }

  float progress =
      static_cast<float>((t - keyframes_[i]->Time()) /
                         (keyframes_[i+1]->Time() - keyframes_[i]->Time()));

  if (keyframes_[i]->timing_function())
    progress = keyframes_[i]->timing_function()->GetValue(progress);

  return keyframes_[i]->Value() +
      (keyframes_[i+1]->Value() - keyframes_[i]->Value()) * progress;
}

scoped_ptr<KeyframedTransformAnimationCurve> KeyframedTransformAnimationCurve::
    Create() {
  return make_scoped_ptr(new KeyframedTransformAnimationCurve);
}

KeyframedTransformAnimationCurve::KeyframedTransformAnimationCurve() {}

KeyframedTransformAnimationCurve::~KeyframedTransformAnimationCurve() {}

void KeyframedTransformAnimationCurve::AddKeyframe(
    scoped_ptr<TransformKeyframe> keyframe) {
  InsertKeyframe(keyframe.Pass(), keyframes_);
}

double KeyframedTransformAnimationCurve::Duration() const {
  return keyframes_.back()->Time() - keyframes_.front()->Time();
}

scoped_ptr<AnimationCurve> KeyframedTransformAnimationCurve::Clone() const {
  scoped_ptr<KeyframedTransformAnimationCurve> to_return(
      KeyframedTransformAnimationCurve::Create());
  for (size_t i = 0; i < keyframes_.size(); ++i)
    to_return->AddKeyframe(keyframes_[i]->Clone());
  return to_return.PassAs<AnimationCurve>();
}

gfx::Transform KeyframedTransformAnimationCurve::GetValue(double t) const {
  if (t <= keyframes_.front()->Time())
    return keyframes_.front()->Value().Apply();

  if (t >= keyframes_.back()->Time())
    return keyframes_.back()->Value().Apply();

  size_t i = 0;
  for (; i < keyframes_.size() - 1; ++i) {
    if (t < keyframes_[i+1]->Time())
      break;
  }

  double progress = (t - keyframes_[i]->Time()) /
                    (keyframes_[i+1]->Time() - keyframes_[i]->Time());

  if (keyframes_[i]->timing_function())
    progress = keyframes_[i]->timing_function()->GetValue(progress);

  return keyframes_[i+1]->Value().Blend(keyframes_[i]->Value(), progress);
}

}  // namespace cc
