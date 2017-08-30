// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ANIMATION_PLAYER_H_
#define CHROME_BROWSER_VR_ANIMATION_PLAYER_H_

#include <set>
#include <vector>

#include "base/macros.h"
#include "cc/animation/animation.h"
#include "chrome/browser/vr/transition.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {
class AnimationTarget;
class TransformOperations;
}  // namespace cc

namespace gfx {
class SizeF;
}  // namespace gfx

namespace vr {

// This is a simplified version of the cc::AnimationPlayer. Its sole purpose is
// the management of its collection of animations. Ticking them, updating their
// state, and deleting them as required.
//
// TODO(vollick): if cc::Animation and friends move into gfx/, then this class
// should follow suit. As such, it should not absorb any vr-specific
// functionality.
class AnimationPlayer final {
 public:
  AnimationPlayer();
  ~AnimationPlayer();

  static int GetNextAnimationId();
  static int GetNextGroupId();

  cc::AnimationTarget* target() const { return target_; }
  void set_target(cc::AnimationTarget* target) { target_ = target; }

  // Animations for properties already being animated are enqueued. Animation
  // groups are not currently supported. crbug.com/742358
  void AddAnimation(std::unique_ptr<cc::Animation> animation);
  void RemoveAnimation(int animation_id);
  void RemoveAnimations(int target_property);

  void Tick(base::TimeTicks monotonic_time);

  using Animations = std::vector<std::unique_ptr<cc::Animation>>;
  const Animations& animations() { return animations_; }

  // The transition is analogous to CSS transitions. When configured, the
  // transition object will cause subsequent calls the corresponding
  // TransitionXXXTo functions to induce transition animations.
  const Transition& transition() const { return transition_; }
  void set_transition(const Transition& transition) {
    transition_ = transition;
  }

  void SetTransitionedProperties(const std::set<int>& properties);

  // TODO(754820): Remove duplicate code from the transition functions
  // by using templates.
  void TransitionFloatTo(base::TimeTicks monotonic_time,
                         int target_property,
                         float current,
                         float target);
  void TransitionTransformOperationsTo(base::TimeTicks monotonic_time,
                                       int target_property,
                                       const cc::TransformOperations& current,
                                       const cc::TransformOperations& target);
  void TransitionSizeTo(base::TimeTicks monotonic_time,
                        int target_property,
                        const gfx::SizeF& current,
                        const gfx::SizeF& target);
  void TransitionColorTo(base::TimeTicks monotonic_time,
                         int target_property,
                         SkColor current,
                         SkColor target);

  bool IsAnimatingProperty(int property) const;

  // TODO(754822): Implement target getters for other value types.
  gfx::SizeF GetTargetSizeValue(int target_property) const;

 private:
  void StartAnimations(base::TimeTicks monotonic_time);
  cc::Animation* GetRunningAnimationForProperty(int target_property) const;

  cc::AnimationTarget* target_ = nullptr;
  Animations animations_;
  Transition transition_;

  DISALLOW_COPY_AND_ASSIGN(AnimationPlayer);
};

}  // namespace vr

#endif  //  CHROME_BROWSER_VR_ANIMATION_PLAYER_H_
