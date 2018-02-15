// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SINGLE_KEYFRAME_EFFECT_ANIMATION_PLAYER_H_
#define CC_ANIMATION_SINGLE_KEYFRAME_EFFECT_ANIMATION_PLAYER_H_

#include <vector>

#include <memory>
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/keyframe_model.h"
#include "cc/trees/element_id.h"

namespace cc {

class KeyframeEffect;

// SingleKeyframeEffectAnimationPlayer is a sub-class of AnimationPlayer. It
// serves as a bridge between the cc animation clients and cc because we
// previously only supported single effect(keyframe_effect) per player.
//
// There is a 1:1 relationship between SingleKeyframeEffectAnimationPlayer and
// the KeyframeEffect. In general, the base class AnimationPlayer is a 1:N
// relationship to allow for grouped animations.
//
// TODO(yigu): Deprecate SingleKeyframeEffectAnimationPlayer once grouped
// animations are fully supported by all clients.
class CC_ANIMATION_EXPORT SingleKeyframeEffectAnimationPlayer
    : public AnimationPlayer {
 public:
  static scoped_refptr<SingleKeyframeEffectAnimationPlayer> Create(int id);
  scoped_refptr<AnimationPlayer> CreateImplInstance() const override;

  ElementId element_id() const;

  void AttachElement(ElementId element_id);

  KeyframeEffect* keyframe_effect() const;
  void AddKeyframeModel(std::unique_ptr<KeyframeModel> keyframe_model);
  void PauseKeyframeModel(int keyframe_model_id, double time_offset);
  void RemoveKeyframeModel(int keyframe_model_id);
  void AbortKeyframeModel(int keyframe_model_id);

  bool NotifyKeyframeModelFinishedForTesting(
      TargetProperty::Type target_property,
      int group_id);
  KeyframeModel* GetKeyframeModel(TargetProperty::Type target_property) const;

 private:
  friend class base::RefCounted<SingleKeyframeEffectAnimationPlayer>;

  KeyframeEffect* GetKeyframeEffect() const;

 protected:
  explicit SingleKeyframeEffectAnimationPlayer(int id);
  explicit SingleKeyframeEffectAnimationPlayer(int id,
                                               size_t keyframe_effect_id);
  ~SingleKeyframeEffectAnimationPlayer() override;

  DISALLOW_COPY_AND_ASSIGN(SingleKeyframeEffectAnimationPlayer);
};

}  // namespace cc

#endif  // CC_ANIMATION_SINGLE_KEYFRAME_EFFECT_ANIMATION_PLAYER_H_
