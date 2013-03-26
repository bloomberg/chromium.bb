// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_ANIMATION_TEST_COMMON_H_
#define CC_TEST_ANIMATION_TEST_COMMON_H_

#include "cc/animation/animation.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/layer_animation_controller.h"
#include "cc/animation/layer_animation_value_observer.h"

namespace cc {
class LayerImpl;
class Layer;
}

namespace cc {

class FakeFloatAnimationCurve : public FloatAnimationCurve {
 public:
  FakeFloatAnimationCurve();
  explicit FakeFloatAnimationCurve(double duration);
  virtual ~FakeFloatAnimationCurve();

  virtual double Duration() const OVERRIDE;
  virtual float GetValue(double now) const OVERRIDE;
  virtual scoped_ptr<AnimationCurve> Clone() const OVERRIDE;

 private:
  double duration_;
};

class FakeTransformTransition : public TransformAnimationCurve {
 public:
  explicit FakeTransformTransition(double duration);
  virtual ~FakeTransformTransition();

  virtual double Duration() const OVERRIDE;
  virtual gfx::Transform GetValue(double time) const OVERRIDE;

  virtual scoped_ptr<AnimationCurve> Clone() const OVERRIDE;

 private:
  double duration_;
};

class FakeFloatTransition : public FloatAnimationCurve {
 public:
  FakeFloatTransition(double duration, float from, float to);
  virtual ~FakeFloatTransition();

  virtual double Duration() const OVERRIDE;
  virtual float GetValue(double time) const OVERRIDE;

  virtual scoped_ptr<AnimationCurve> Clone() const OVERRIDE;

 private:
  double duration_;
  float from_;
  float to_;
};

class FakeLayerAnimationValueObserver : public LayerAnimationValueObserver {
 public:
  FakeLayerAnimationValueObserver();
  virtual ~FakeLayerAnimationValueObserver();

  // LayerAnimationValueObserver implementation
  virtual void OnOpacityAnimated(float opacity) OVERRIDE;
  virtual void OnTransformAnimated(const gfx::Transform& transform) OVERRIDE;
  virtual bool IsActive() const OVERRIDE;

  float opacity() const  { return opacity_; }
  const gfx::Transform& transform() const { return transform_; }

 private:
  float opacity_;
  gfx::Transform transform_;
};

int AddOpacityTransitionToController(LayerAnimationController* controller,
                                     double duration,
                                     float start_opacity,
                                     float end_opacity,
                                     bool use_timing_function);

int AddAnimatedTransformToController(LayerAnimationController* controller,
                                     double duration,
                                     int delta_x,
                                     int delta_y);

int AddOpacityTransitionToLayer(Layer* layer,
                                double duration,
                                float start_opacity,
                                float end_opacity,
                                bool use_timing_function);

int AddOpacityTransitionToLayer(LayerImpl* layer,
                                double duration,
                                float start_opacity,
                                float end_opacity,
                                bool use_timing_function);

int AddAnimatedTransformToLayer(Layer* layer,
                                double duration,
                                int delta_x,
                                int delta_y);

int AddAnimatedTransformToLayer(LayerImpl* layer,
                                double duration,
                                int delta_x,
                                int delta_y);

}  // namespace cc

#endif  // CC_TEST_ANIMATION_TEST_COMMON_H_
