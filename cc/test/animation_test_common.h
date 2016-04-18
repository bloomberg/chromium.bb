// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_ANIMATION_TEST_COMMON_H_
#define CC_TEST_ANIMATION_TEST_COMMON_H_

#include "cc/animation/animation.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/layer_animation_controller.h"
#include "cc/animation/layer_animation_value_observer.h"
#include "cc/animation/layer_animation_value_provider.h"
#include "cc/animation/transform_operations.h"
#include "cc/output/filter_operations.h"
#include "cc/test/geometry_test_utils.h"

namespace cc {
class AnimationPlayer;
class LayerImpl;
class Layer;
}

namespace cc {

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
  gfx::Transform GetValue(base::TimeDelta time) const override;
  bool AnimatedBoundsForBox(const gfx::BoxF& box,
                            gfx::BoxF* bounds) const override;
  bool AffectsScale() const override;
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

class FakeLayerAnimationValueObserver : public LayerAnimationValueObserver {
 public:
  FakeLayerAnimationValueObserver();
  ~FakeLayerAnimationValueObserver() override;

  // LayerAnimationValueObserver implementation.
  void OnFilterAnimated(LayerTreeType tree_type,
                        const FilterOperations& filters) override;
  void OnOpacityAnimated(LayerTreeType tree_type, float opacity) override;
  void OnTransformAnimated(LayerTreeType tree_type,
                           const gfx::Transform& transform) override;
  void OnScrollOffsetAnimated(LayerTreeType tree_type,
                              const gfx::ScrollOffset& scroll_offset) override;
  void OnAnimationWaitingForDeletion() override;
  void OnTransformIsPotentiallyAnimatingChanged(LayerTreeType tree_type,
                                                bool is_animating) override;

  const FilterOperations& filters(LayerTreeType tree_type) const {
    return filters_[ToIndex(tree_type)];
  }
  float opacity(LayerTreeType tree_type) const {
    return opacity_[ToIndex(tree_type)];
  }
  const gfx::Transform& transform(LayerTreeType tree_type) const {
    return transform_[ToIndex(tree_type)];
  }
  gfx::ScrollOffset scroll_offset(LayerTreeType tree_type) {
    return scroll_offset_[ToIndex(tree_type)];
  }

  bool animation_waiting_for_deletion() {
    return animation_waiting_for_deletion_;
  }

  bool transform_is_animating(LayerTreeType tree_type) {
    return transform_is_animating_[ToIndex(tree_type)];
  }

 private:
  static int ToIndex(LayerTreeType tree_type);

  FilterOperations filters_[2];
  float opacity_[2];
  gfx::Transform transform_[2];
  gfx::ScrollOffset scroll_offset_[2];
  bool animation_waiting_for_deletion_;
  bool transform_is_animating_[2];
};

class FakeLayerAnimationValueProvider : public LayerAnimationValueProvider {
 public:
  gfx::ScrollOffset ScrollOffsetForAnimation() const override;

  void set_scroll_offset(const gfx::ScrollOffset& scroll_offset) {
    scroll_offset_ = scroll_offset;
  }

 private:
  gfx::ScrollOffset scroll_offset_;
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

int AddAnimatedFilterToController(LayerAnimationController* controller,
                                  double duration,
                                  float start_brightness,
                                  float end_brightness);

int AddAnimatedTransformToPlayer(AnimationPlayer* player,
                                 double duration,
                                 int delta_x,
                                 int delta_y);

int AddAnimatedTransformToPlayer(AnimationPlayer* player,
                                 double duration,
                                 TransformOperations start_operations,
                                 TransformOperations operations);

int AddOpacityTransitionToPlayer(AnimationPlayer* player,
                                 double duration,
                                 float start_opacity,
                                 float end_opacity,
                                 bool use_timing_function);

int AddAnimatedFilterToPlayer(AnimationPlayer* player,
                              double duration,
                              float start_brightness,
                              float end_brightness);

int AddOpacityStepsToController(LayerAnimationController* target,
                                double duration,
                                float start_opacity,
                                float end_opacity,
                                int num_steps);

void AddAnimationToLayerWithPlayer(int layer_id,
                                   scoped_refptr<AnimationTimeline> timeline,
                                   std::unique_ptr<Animation> animation);
void AddAnimationToLayerWithExistingPlayer(
    int layer_id,
    scoped_refptr<AnimationTimeline> timeline,
    std::unique_ptr<Animation> animation);

void RemoveAnimationFromLayerWithExistingPlayer(
    int layer_id,
    scoped_refptr<AnimationTimeline> timeline,
    int animation_id);

Animation* GetAnimationFromLayerWithExistingPlayer(
    int layer_id,
    scoped_refptr<AnimationTimeline> timeline,
    int animation_id);

int AddAnimatedFilterToLayerWithPlayer(
    int layer_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    float start_brightness,
    float end_brightness);

int AddAnimatedTransformToLayerWithPlayer(
    int layer_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    int delta_x,
    int delta_y);

int AddAnimatedTransformToLayerWithPlayer(
    int layer_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    TransformOperations start_operations,
    TransformOperations operations);

int AddOpacityTransitionToLayerWithPlayer(
    int layer_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    float start_opacity,
    float end_opacity,
    bool use_timing_function);

void AbortAnimationsOnLayerWithPlayer(int layer_id,
                                      scoped_refptr<AnimationTimeline> timeline,
                                      TargetProperty::Type target_property);

}  // namespace cc

#endif  // CC_TEST_ANIMATION_TEST_COMMON_H_
