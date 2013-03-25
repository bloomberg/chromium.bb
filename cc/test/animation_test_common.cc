// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/animation_test_common.h"

#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/layer_animation_controller.h"
#include "cc/animation/transform_operations.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"

using cc::Animation;
using cc::AnimationCurve;
using cc::EaseTimingFunction;
using cc::FloatKeyframe;
using cc::KeyframedFloatAnimationCurve;
using cc::KeyframedTransformAnimationCurve;
using cc::TimingFunction;
using cc::TransformKeyframe;

namespace cc {

static int s_next_animation_id = 0;

template <class Target>
int AddOpacityTransition(Target* target,
                         double duration,
                         float start_opacity,
                         float end_opacity,
                         bool use_timing_function) {
  scoped_ptr<KeyframedFloatAnimationCurve>
      curve(KeyframedFloatAnimationCurve::Create());

  scoped_ptr<TimingFunction> func;
  if (!use_timing_function)
    func = EaseTimingFunction::Create();
  if (duration > 0.0)
    curve->AddKeyframe(FloatKeyframe::Create(0.0, start_opacity, func.Pass()));
  curve->AddKeyframe(FloatKeyframe::Create(duration,
                                           end_opacity,
                                           scoped_ptr<cc::TimingFunction>()));

  int id = s_next_animation_id++;

  scoped_ptr<Animation> animation(Animation::Create(
      curve.PassAs<AnimationCurve>(),
      id,
      0,
      Animation::Opacity));
  animation->set_needs_synchronized_start_time(true);

  target->AddAnimation(animation.Pass());
  return id;
}

template <class Target>
int AddAnimatedTransform(Target* target,
                         double duration,
                         int delta_x,
                         int delta_y) {
  scoped_ptr<KeyframedTransformAnimationCurve>
      curve(KeyframedTransformAnimationCurve::Create());

  if (duration > 0.0) {
    TransformOperations start_operations;
    start_operations.AppendTranslate(delta_x, delta_y, 0.0);
    curve->AddKeyframe(TransformKeyframe::Create(
        0.0,
        start_operations,
        scoped_ptr<cc::TimingFunction>()));
  }

  TransformOperations operations;
  operations.AppendTranslate(delta_x, delta_y, 0.0);
  curve->AddKeyframe(TransformKeyframe::Create(
      duration,
      operations,
      scoped_ptr<cc::TimingFunction>()));

  int id = s_next_animation_id++;

  scoped_ptr<Animation> animation(Animation::Create(
      curve.PassAs<AnimationCurve>(),
      id, 0, Animation::Transform));
  animation->set_needs_synchronized_start_time(true);

  target->AddAnimation(animation.Pass());
  return id;
}

FakeFloatAnimationCurve::FakeFloatAnimationCurve()
    : duration_(1.0)
{
}

FakeFloatAnimationCurve::FakeFloatAnimationCurve(double duration)
    : duration_(duration)
{
}

FakeFloatAnimationCurve::~FakeFloatAnimationCurve() {
}

double FakeFloatAnimationCurve::Duration() const {
  return duration_;
}

float FakeFloatAnimationCurve::GetValue(double now) const {
  return 0.0f;
}

scoped_ptr<cc::AnimationCurve> FakeFloatAnimationCurve::Clone() const {
  return make_scoped_ptr(
      new FakeFloatAnimationCurve).PassAs<cc::AnimationCurve>();
}

FakeTransformTransition::FakeTransformTransition(double duration)
    : duration_(duration)
{
}

FakeTransformTransition::~FakeTransformTransition() {
}

double FakeTransformTransition::Duration() const {
  return duration_;
}

gfx::Transform FakeTransformTransition::GetValue(double time) const {
  return gfx::Transform();
}

scoped_ptr<cc::AnimationCurve> FakeTransformTransition::Clone() const {
  return make_scoped_ptr(
      new FakeTransformTransition(*this)).PassAs<cc::AnimationCurve>();
}


FakeFloatTransition::FakeFloatTransition(double duration, float from, float to)
    : duration_(duration)
    , from_(from)
    , to_(to)
{
}

FakeFloatTransition::~FakeFloatTransition() {
}

double FakeFloatTransition::Duration() const {
  return duration_;
}

float FakeFloatTransition::GetValue(double time) const {
  time /= duration_;
  if (time >= 1.0)
    time = 1.0;
  return (1.0 - time) * from_ + time * to_;
}

FakeLayerAnimationValueObserver::FakeLayerAnimationValueObserver()
    : opacity_(0.0f)
{
}

FakeLayerAnimationValueObserver::~FakeLayerAnimationValueObserver() {
}

void FakeLayerAnimationValueObserver::OnOpacityAnimated(float opacity) {
  opacity_ = opacity;
}

void FakeLayerAnimationValueObserver::OnTransformAnimated(
    const gfx::Transform& transform) {
  transform_ = transform;
}

bool FakeLayerAnimationValueObserver::IsActive() const {
  return true;
}

scoped_ptr<cc::AnimationCurve> FakeFloatTransition::Clone() const {
  return make_scoped_ptr(
      new FakeFloatTransition(*this)).PassAs<cc::AnimationCurve>();
}

int AddOpacityTransitionToController(cc::LayerAnimationController* controller,
                                     double duration,
                                     float start_opacity,
                                     float end_opacity,
                                     bool use_timing_function) {
  return AddOpacityTransition(controller,
                              duration,
                              start_opacity,
                              end_opacity,
                              use_timing_function);
}

int AddAnimatedTransformToController(cc::LayerAnimationController* controller,
                                     double duration,
                                     int delta_x,
                                     int delta_y) {
  return AddAnimatedTransform(controller,
                              duration,
                              delta_x,
                              delta_y);
}

int AddOpacityTransitionToLayer(cc::Layer* layer,
                                double duration,
                                float start_opacity,
                                float end_opacity,
                                bool use_timing_function) {
  return AddOpacityTransition(layer,
                              duration,
                              start_opacity,
                              end_opacity,
                              use_timing_function);
}

int AddOpacityTransitionToLayer(cc::LayerImpl* layer,
                                double duration,
                                float start_opacity,
                                float end_opacity,
                                bool use_timing_function) {
  return AddOpacityTransition(layer->layer_animation_controller(),
                              duration,
                              start_opacity,
                              end_opacity,
                              use_timing_function);
}

int AddAnimatedTransformToLayer(cc::Layer* layer,
                                double duration,
                                int delta_x,
                                int delta_y) {
  return AddAnimatedTransform(layer, duration, delta_x, delta_y);
}

int AddAnimatedTransformToLayer(cc::LayerImpl* layer,
                                double duration,
                                int delta_x,
                                int delta_y) {
  return AddAnimatedTransform(layer->layer_animation_controller(),
                              duration,
                              delta_x,
                              delta_y);
}

}  // namespace cc
