// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_PLAYER_H_
#define CC_ANIMATION_ANIMATION_PLAYER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/element_animations.h"
#include "cc/trees/element_id.h"

namespace cc {

class AnimationDelegate;
class AnimationEvents;
class AnimationHost;
class AnimationTimeline;
class AnimationTicker;
struct AnimationEvent;
struct PropertyAnimationState;

// An AnimationPlayer manages grouped sets of animations (each set of which are
// stored in an AnimationTicker), and handles the interaction with the
// AnimationHost and AnimationTimeline.
//
// This class is a CC counterpart for blink::Animation, currently in a 1:1
// relationship. Currently the blink logic is responsible for handling of
// conflicting same-property animations.
//
// Each cc AnimationPlayer has a copy on the impl thread, and will take care of
// synchronizing properties to/from the impl thread when requested.
//
// NOTE(smcgruer): As of 2017/09/06 there is a 1:1 relationship between
// AnimationPlayer and the AnimationTicker. This is intended to become a 1:N
// relationship to allow for grouped animations.
class CC_ANIMATION_EXPORT AnimationPlayer
    : public base::RefCounted<AnimationPlayer> {
 public:
  static scoped_refptr<AnimationPlayer> Create(int id);
  scoped_refptr<AnimationPlayer> CreateImplInstance() const;

  int id() const { return id_; }
  ElementId element_id() const;

  // Parent AnimationHost. AnimationPlayer can be detached from
  // AnimationTimeline.
  AnimationHost* animation_host() { return animation_host_; }
  const AnimationHost* animation_host() const { return animation_host_; }
  void SetAnimationHost(AnimationHost* animation_host);
  bool has_animation_host() const { return !!animation_host_; }

  // Parent AnimationTimeline.
  AnimationTimeline* animation_timeline() { return animation_timeline_; }
  const AnimationTimeline* animation_timeline() const {
    return animation_timeline_;
  }
  void SetAnimationTimeline(AnimationTimeline* timeline);

  scoped_refptr<ElementAnimations> element_animations() const;

  void set_animation_delegate(AnimationDelegate* delegate) {
    animation_delegate_ = delegate;
  }

  void AttachElement(ElementId element_id);
  void DetachElement();

  void AddAnimation(std::unique_ptr<Animation> animation);
  void PauseAnimation(int animation_id, double time_offset);
  void RemoveAnimation(int animation_id);
  void AbortAnimation(int animation_id);
  void AbortAnimations(TargetProperty::Type target_property,
                       bool needs_completion);

  void PushPropertiesTo(AnimationPlayer* player_impl);

  void Tick(base::TimeTicks monotonic_time);
  void UpdateState(bool start_ready_animations, AnimationEvents* events);

  void UpdateTickingState(UpdateTickingType type);
  void AddToTicking();
  void RemoveFromTicking();

  // AnimationDelegate routing.
  bool NotifyAnimationStarted(const AnimationEvent& event);
  bool NotifyAnimationFinished(const AnimationEvent& event);
  bool NotifyAnimationAborted(const AnimationEvent& event);
  void NotifyAnimationTakeover(const AnimationEvent& event);
  bool NotifyAnimationFinishedForTesting(TargetProperty::Type target_property,
                                         int group_id);
  // TODO(smcgruer): Once ElementAnimations points at AnimationTicker directly,
  // merge these methods with the above.
  void NotifyImplOnlyAnimationStarted(AnimationEvent&);
  void NotifyImplOnlyAnimationFinished(AnimationEvent&);
  void NotifyAnimationTakeoverByMain(AnimationEvent&);

  // Returns true if there are any animations that have neither finished nor
  // aborted.
  bool HasTickingAnimation() const;

  // Returns true if there are any animations at all to process.
  bool has_any_animation() const;

  bool needs_push_properties() const { return needs_push_properties_; }
  void SetNeedsPushProperties();

  // Make animations affect active elements if and only if they affect
  // pending elements. Any animations that no longer affect any elements
  // are deleted.
  void ActivateAnimations();

  bool HasOnlyTranslationTransforms(ElementListType list_type) const;
  bool AnimationsPreserveAxisAlignment() const;

  // Sets |start_scale| to the maximum of starting animation scale along any
  // dimension at any destination in active animations. Returns false if the
  // starting scale cannot be computed.
  bool AnimationStartScale(ElementListType list_type, float* start_scale) const;

  // Sets |max_scale| to the maximum scale along any dimension at any
  // destination in active animations. Returns false if the maximum scale cannot
  // be computed.
  bool MaximumTargetScale(ElementListType list_type, float* max_scale) const;

  // Returns true if there is an animation that is either currently animating
  // the given property or scheduled to animate this property in the future, and
  // that affects the given tree type.
  bool IsPotentiallyAnimatingProperty(TargetProperty::Type target_property,
                                      ElementListType list_type) const;

  // Returns true if there is an animation that is currently animating the given
  // property and that affects the given tree type.
  bool IsCurrentlyAnimatingProperty(TargetProperty::Type target_property,
                                    ElementListType list_type) const;

  // Returns the animation animating the given property that is either
  // running, or is next to run, if such an animation exists.
  Animation* GetAnimation(TargetProperty::Type target_property) const;

  // Returns animation for the given unique animation id.
  // TODO(smcgruer): Remove, only tests call this and they should call on the
  // AnimationTicker instead.
  Animation* GetAnimationById(int animation_id) const;

  void GetPropertyAnimationState(PropertyAnimationState* pending_state,
                                 PropertyAnimationState* active_state) const;

  bool scroll_offset_animation_was_interrupted() const;

  std::string ToString() const;

  void SetNeedsCommit();

 private:
  friend class base::RefCounted<AnimationPlayer>;

  explicit AnimationPlayer(int id);
  ~AnimationPlayer();

  void RegisterPlayer();
  void UnregisterPlayer();

  void BindElementAnimations();
  void UnbindElementAnimations();

  void PushPropertiesToImplThread(AnimationPlayer* animation_player_impl);

  AnimationHost* animation_host_;
  AnimationTimeline* animation_timeline_;
  AnimationDelegate* animation_delegate_;

  int id_;
  bool needs_push_properties_;

  std::unique_ptr<AnimationTicker> animation_ticker_;

  DISALLOW_COPY_AND_ASSIGN(AnimationPlayer);
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_PLAYER_H_
