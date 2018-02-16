// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/single_keyframe_effect_animation_player.h"

#include <inttypes.h>
#include <algorithm>

#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/transform_operations.h"
#include "cc/trees/property_animation_state.h"

namespace cc {

scoped_refptr<SingleKeyframeEffectAnimationPlayer>
SingleKeyframeEffectAnimationPlayer::Create(int id) {
  return base::WrapRefCounted(new SingleKeyframeEffectAnimationPlayer(id));
}

SingleKeyframeEffectAnimationPlayer::SingleKeyframeEffectAnimationPlayer(int id)
    : AnimationPlayer(id) {
  DCHECK(id_);
  AddKeyframeEffect(base::MakeUnique<KeyframeEffect>(NextKeyframeEffectId()));
}

SingleKeyframeEffectAnimationPlayer::SingleKeyframeEffectAnimationPlayer(
    int id,
    size_t keyframe_effect_id)
    : AnimationPlayer(id) {
  DCHECK(id_);
  AddKeyframeEffect(base::MakeUnique<KeyframeEffect>(keyframe_effect_id));
}

SingleKeyframeEffectAnimationPlayer::~SingleKeyframeEffectAnimationPlayer() {}

KeyframeEffect* SingleKeyframeEffectAnimationPlayer::GetKeyframeEffect() const {
  DCHECK_EQ(keyframe_effects_.size(), 1u);
  return keyframe_effects_[0].get();
}

scoped_refptr<AnimationPlayer>
SingleKeyframeEffectAnimationPlayer::CreateImplInstance() const {
  DCHECK(GetKeyframeEffect());
  scoped_refptr<SingleKeyframeEffectAnimationPlayer> player =
      base::WrapRefCounted(new SingleKeyframeEffectAnimationPlayer(
          id(), GetKeyframeEffect()->id()));
  return player;
}

ElementId SingleKeyframeEffectAnimationPlayer::element_id() const {
  return element_id_of_keyframe_effect(GetKeyframeEffect()->id());
}

void SingleKeyframeEffectAnimationPlayer::AttachElement(ElementId element_id) {
  AttachElementForKeyframeEffect(element_id, GetKeyframeEffect()->id());
}

KeyframeEffect* SingleKeyframeEffectAnimationPlayer::keyframe_effect() const {
  return GetKeyframeEffect();
}

void SingleKeyframeEffectAnimationPlayer::AddKeyframeModel(
    std::unique_ptr<KeyframeModel> keyframe_model) {
  AddKeyframeModelForKeyframeEffect(std::move(keyframe_model),
                                    GetKeyframeEffect()->id());
}

void SingleKeyframeEffectAnimationPlayer::PauseKeyframeModel(
    int keyframe_model_id,
    double time_offset) {
  PauseKeyframeModelForKeyframeEffect(keyframe_model_id, time_offset,
                                      GetKeyframeEffect()->id());
}

void SingleKeyframeEffectAnimationPlayer::RemoveKeyframeModel(
    int keyframe_model_id) {
  RemoveKeyframeModelForKeyframeEffect(keyframe_model_id,
                                       GetKeyframeEffect()->id());
}

void SingleKeyframeEffectAnimationPlayer::AbortKeyframeModel(
    int keyframe_model_id) {
  AbortKeyframeModelForKeyframeEffect(keyframe_model_id,
                                      GetKeyframeEffect()->id());
}

bool SingleKeyframeEffectAnimationPlayer::NotifyKeyframeModelFinishedForTesting(
    TargetProperty::Type target_property,
    int group_id) {
  AnimationEvent event(AnimationEvent::FINISHED,
                       GetKeyframeEffect()->element_id(), group_id,
                       target_property, base::TimeTicks());
  return GetKeyframeEffect()->NotifyKeyframeModelFinished(event);
}

KeyframeModel* SingleKeyframeEffectAnimationPlayer::GetKeyframeModel(
    TargetProperty::Type target_property) const {
  return GetKeyframeModelForKeyframeEffect(target_property,
                                           GetKeyframeEffect()->id());
}

}  // namespace cc
