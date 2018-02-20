// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_ANIMATION_TEST_COMMON_H_
#define CC_TEST_ANIMATION_TEST_COMMON_H_

#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_model.h"
#include "cc/animation/transform_operations.h"
#include "cc/paint/filter_operations.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/element_id.h"

namespace gfx {
class ScrollOffset;
}

namespace cc {

class SingleKeyframeEffectAnimationPlayer;

class FakeFloatAnimationCurve : public FloatAnimationCurve {
 public:
  FakeFloatAnimationCurve();
  explicit FakeFloatAnimationCurve(double duration);
  ~FakeFloatAnimationCurve() override;

  base::TimeDelta Duration() const override;
  float GetValue(base::TimeDelta now) const override;
  std::unique_ptr<AnimationCurve> Clone() const override;

 private:
  base::TimeDelta duration_;
};

class FakeTransformTransition : public TransformAnimationCurve {
 public:
  explicit FakeTransformTransition(double duration);
  ~FakeTransformTransition() override;

  base::TimeDelta Duration() const override;
  TransformOperations GetValue(base::TimeDelta time) const override;
  bool AnimatedBoundsForBox(const gfx::BoxF& box,
                            gfx::BoxF* bounds) const override;
  bool IsTranslation() const override;
  bool PreservesAxisAlignment() const override;
  bool AnimationStartScale(bool forward_direction,
                           float* start_scale) const override;
  bool MaximumTargetScale(bool forward_direction,
                          float* max_scale) const override;

  std::unique_ptr<AnimationCurve> Clone() const override;

 private:
  base::TimeDelta duration_;
};

class FakeFloatTransition : public FloatAnimationCurve {
 public:
  FakeFloatTransition(double duration, float from, float to);
  ~FakeFloatTransition() override;

  base::TimeDelta Duration() const override;
  float GetValue(base::TimeDelta time) const override;

  std::unique_ptr<AnimationCurve> Clone() const override;

 private:
  base::TimeDelta duration_;
  float from_;
  float to_;
};

int AddScrollOffsetAnimationToPlayer(
    SingleKeyframeEffectAnimationPlayer* player,
    gfx::ScrollOffset initial_value,
    gfx::ScrollOffset target_value);

int AddAnimatedTransformToPlayer(SingleKeyframeEffectAnimationPlayer* player,
                                 double duration,
                                 int delta_x,
                                 int delta_y);

int AddAnimatedTransformToPlayer(SingleKeyframeEffectAnimationPlayer* player,
                                 double duration,
                                 TransformOperations start_operations,
                                 TransformOperations operations);

int AddOpacityTransitionToPlayer(SingleKeyframeEffectAnimationPlayer* player,
                                 double duration,
                                 float start_opacity,
                                 float end_opacity,
                                 bool use_timing_function);

int AddAnimatedFilterToPlayer(SingleKeyframeEffectAnimationPlayer* player,
                              double duration,
                              float start_brightness,
                              float end_brightness);

int AddOpacityStepsToPlayer(SingleKeyframeEffectAnimationPlayer* player,
                            double duration,
                            float start_opacity,
                            float end_opacity,
                            int num_steps);

void AddKeyframeModelToElementWithPlayer(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    std::unique_ptr<KeyframeModel> keyframe_model);
void AddKeyframeModelToElementWithExistingKeyframeEffect(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    std::unique_ptr<KeyframeModel> keyframe_model);

void RemoveKeyframeModelFromElementWithExistingKeyframeEffect(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    int keyframe_model_id);

KeyframeModel* GetKeyframeModelFromElementWithExistingKeyframeEffect(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    int keyframe_model_id);

int AddAnimatedFilterToElementWithPlayer(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    float start_brightness,
    float end_brightness);

int AddAnimatedTransformToElementWithPlayer(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    int delta_x,
    int delta_y);

int AddAnimatedTransformToElementWithPlayer(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    TransformOperations start_operations,
    TransformOperations operations);

int AddOpacityTransitionToElementWithPlayer(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    float start_opacity,
    float end_opacity,
    bool use_timing_function);

}  // namespace cc

#endif  // CC_TEST_ANIMATION_TEST_COMMON_H_
