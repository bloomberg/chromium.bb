// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_PLAYER_H_
#define CC_ANIMATION_ANIMATION_PLAYER_H_

#include <vector>

#include <memory>
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/keyframe_model.h"
#include "cc/trees/element_id.h"

namespace cc {

class AnimationDelegate;
class AnimationEvents;
class AnimationHost;
class AnimationTimeline;
struct AnimationEvent;

// An AnimationPlayer manages grouped sets of KeyframeModels (each set of which
// are stored in a KeyframeEffect), and handles the interaction with the
// AnimationHost and AnimationTimeline.
//
// This class is a CC counterpart for blink::Animation, currently in a 1:1
// relationship. Currently the blink logic is responsible for handling of
// conflicting same-property animations.
//
// Each cc AnimationPlayer has a copy on the impl thread, and will take care of
// synchronizing properties to/from the impl thread when requested.
//
// There is a 1:n relationship between AnimationPlayer and KeyframeEffect.
class CC_ANIMATION_EXPORT AnimationPlayer
    : public base::RefCounted<AnimationPlayer> {
 public:
  static scoped_refptr<AnimationPlayer> Create(int id);
  virtual scoped_refptr<AnimationPlayer> CreateImplInstance() const;

  int id() const { return id_; }
  typedef size_t KeyframeEffectId;
  ElementId element_id_of_keyframe_effect(
      KeyframeEffectId keyframe_effect_id) const;
  bool IsElementAttached(ElementId id) const;

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
  virtual void SetAnimationTimeline(AnimationTimeline* timeline);

  bool has_element_animations() const;
  scoped_refptr<ElementAnimations> element_animations(
      KeyframeEffectId keyframe_effect_id) const;

  void set_animation_delegate(AnimationDelegate* delegate) {
    animation_delegate_ = delegate;
  }

  void AttachElementForKeyframeEffect(ElementId element_id,
                                      KeyframeEffectId keyframe_effect_id);
  void DetachElementForKeyframeEffect(ElementId element_id,
                                      KeyframeEffectId keyframe_effect_id);
  virtual void DetachElement();

  void AddKeyframeModelForKeyframeEffect(
      std::unique_ptr<KeyframeModel> keyframe_model,
      KeyframeEffectId keyframe_effect_id);
  void PauseKeyframeModelForKeyframeEffect(int keyframe_model_id,
                                           double time_offset,
                                           KeyframeEffectId keyframe_effect_id);
  void RemoveKeyframeModelForKeyframeEffect(
      int keyframe_model_id,
      KeyframeEffectId keyframe_effect_id);
  void AbortKeyframeModelForKeyframeEffect(int keyframe_model_id,
                                           KeyframeEffectId keyframe_effect_id);
  void AbortKeyframeModels(TargetProperty::Type target_property,
                           bool needs_completion);

  virtual void PushPropertiesTo(AnimationPlayer* player_impl);

  void UpdateState(bool start_ready_keyframe_models, AnimationEvents* events);
  virtual void Tick(base::TimeTicks monotonic_time);

  void AddToTicking();
  void KeyframeModelRemovedFromTicking();

  // AnimationDelegate routing.
  void NotifyKeyframeModelStarted(const AnimationEvent& event);
  void NotifyKeyframeModelFinished(const AnimationEvent& event);
  void NotifyKeyframeModelAborted(const AnimationEvent& event);
  void NotifyKeyframeModelTakeover(const AnimationEvent& event);
  size_t TickingKeyframeModelsCount() const;

  void SetNeedsPushProperties();

  // Make KeyframeModels affect active elements if and only if they affect
  // pending elements. Any KeyframeModels that no longer affect any elements
  // are deleted.
  void ActivateKeyframeEffects();

  // Returns the keyframe model animating the given property that is either
  // running, or is next to run, if such a keyframe model exists.
  KeyframeModel* GetKeyframeModelForKeyframeEffect(
      TargetProperty::Type target_property,
      KeyframeEffectId keyframe_effect_id) const;

  std::string ToString() const;

  void SetNeedsCommit();

  virtual bool IsWorkletAnimationPlayer() const;
  void AddKeyframeEffect(std::unique_ptr<KeyframeEffect>);

  KeyframeEffect* GetKeyframeEffectById(
      KeyframeEffectId keyframe_effect_id) const;
  KeyframeEffectId NextKeyframeEffectId() { return keyframe_effects_.size(); }

 private:
  friend class base::RefCounted<AnimationPlayer>;

  void RegisterKeyframeEffect(ElementId element_id,
                              KeyframeEffectId keyframe_effect_id);
  void UnregisterKeyframeEffect(ElementId element_id,
                                KeyframeEffectId keyframe_effect_id);
  void RegisterKeyframeEffects();
  void UnregisterKeyframeEffects();

  void PushAttachedKeyframeEffectsToImplThread(AnimationPlayer* player) const;
  void PushPropertiesToImplThread(AnimationPlayer* player);

 protected:
  explicit AnimationPlayer(int id);
  virtual ~AnimationPlayer();

  AnimationHost* animation_host_;
  AnimationTimeline* animation_timeline_;
  AnimationDelegate* animation_delegate_;

  int id_;

  using ElementToKeyframeEffectIdMap =
      std::unordered_map<ElementId,
                         std::unordered_set<KeyframeEffectId>,
                         ElementIdHash>;
  using KeyframeEffects = std::vector<std::unique_ptr<KeyframeEffect>>;

  // It is possible for a keyframe_effect to be in keyframe_effects_ but not in
  // element_to_keyframe_effect_id_map_ but the reverse is not possible.
  ElementToKeyframeEffectIdMap element_to_keyframe_effect_id_map_;
  KeyframeEffects keyframe_effects_;

  int ticking_keyframe_effects_count;

  DISALLOW_COPY_AND_ASSIGN(AnimationPlayer);
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_PLAYER_H_
