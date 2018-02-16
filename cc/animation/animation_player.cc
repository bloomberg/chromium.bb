// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_player.h"

#include <inttypes.h>
#include <algorithm>

#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/transform_operations.h"
#include "cc/trees/property_animation_state.h"

namespace cc {

scoped_refptr<AnimationPlayer> AnimationPlayer::Create(int id) {
  return base::WrapRefCounted(new AnimationPlayer(id));
}

AnimationPlayer::AnimationPlayer(int id)
    : animation_host_(),
      animation_timeline_(),
      animation_delegate_(),
      id_(id),
      ticking_keyframe_effects_count(0) {
  DCHECK(id_);
}

AnimationPlayer::~AnimationPlayer() {
  DCHECK(!animation_timeline_);
}

scoped_refptr<AnimationPlayer> AnimationPlayer::CreateImplInstance() const {
  scoped_refptr<AnimationPlayer> player = AnimationPlayer::Create(id());
  return player;
}

ElementId AnimationPlayer::element_id_of_keyframe_effect(
    KeyframeEffectId keyframe_effect_id) const {
  DCHECK(GetKeyframeEffectById(keyframe_effect_id));
  return GetKeyframeEffectById(keyframe_effect_id)->element_id();
}

bool AnimationPlayer::IsElementAttached(ElementId id) const {
  return !!element_to_keyframe_effect_id_map_.count(id);
}

void AnimationPlayer::SetAnimationHost(AnimationHost* animation_host) {
  animation_host_ = animation_host;
}

void AnimationPlayer::SetAnimationTimeline(AnimationTimeline* timeline) {
  if (animation_timeline_ == timeline)
    return;

  // We need to unregister keyframe_effect to manage ElementAnimations and
  // observers properly.
  if (!element_to_keyframe_effect_id_map_.empty() && animation_host_) {
    // Destroy ElementAnimations or release it if it's still needed.
    UnregisterKeyframeEffects();
  }

  animation_timeline_ = timeline;

  // Register player only if layer AND host attached. Unlike the
  // SingleAnimationPlayer case, all keyframe_effects have been attached to
  // their corresponding elements.
  if (!element_to_keyframe_effect_id_map_.empty() && animation_host_) {
    RegisterKeyframeEffects();
  }
}

bool AnimationPlayer::has_element_animations() const {
  return !element_to_keyframe_effect_id_map_.empty();
}

scoped_refptr<ElementAnimations> AnimationPlayer::element_animations(
    KeyframeEffectId keyframe_effect_id) const {
  return GetKeyframeEffectById(keyframe_effect_id)->element_animations();
}

void AnimationPlayer::AttachElementForKeyframeEffect(
    ElementId element_id,
    KeyframeEffectId keyframe_effect_id) {
  DCHECK(GetKeyframeEffectById(keyframe_effect_id));
  GetKeyframeEffectById(keyframe_effect_id)->AttachElement(element_id);
  element_to_keyframe_effect_id_map_[element_id].emplace(keyframe_effect_id);
  // Register player only if layer AND host attached.
  if (animation_host_) {
    // Create ElementAnimations or re-use existing.
    RegisterKeyframeEffect(element_id, keyframe_effect_id);
  }
}

void AnimationPlayer::DetachElementForKeyframeEffect(
    ElementId element_id,
    KeyframeEffectId keyframe_effect_id) {
  DCHECK(GetKeyframeEffectById(keyframe_effect_id));
  DCHECK_EQ(GetKeyframeEffectById(keyframe_effect_id)->element_id(),
            element_id);

  UnregisterKeyframeEffect(element_id, keyframe_effect_id);
  GetKeyframeEffectById(keyframe_effect_id)->DetachElement();
  element_to_keyframe_effect_id_map_[element_id].erase(keyframe_effect_id);
}

void AnimationPlayer::DetachElement() {
  if (animation_host_) {
    // Destroy ElementAnimations or release it if it's still needed.
    UnregisterKeyframeEffects();
  }

  for (auto pair = element_to_keyframe_effect_id_map_.begin();
       pair != element_to_keyframe_effect_id_map_.end();) {
    for (auto keyframe_effect = pair->second.begin();
         keyframe_effect != pair->second.end();) {
      GetKeyframeEffectById(*keyframe_effect)->DetachElement();
      keyframe_effect = pair->second.erase(keyframe_effect);
    }
    pair = element_to_keyframe_effect_id_map_.erase(pair);
  }
  DCHECK_EQ(element_to_keyframe_effect_id_map_.size(), 0u);
}

void AnimationPlayer::RegisterKeyframeEffect(
    ElementId element_id,
    KeyframeEffectId keyframe_effect_id) {
  DCHECK(animation_host_);
  KeyframeEffect* keyframe_effect = GetKeyframeEffectById(keyframe_effect_id);
  DCHECK(!keyframe_effect->has_bound_element_animations());

  if (!keyframe_effect->has_attached_element())
    return;
  animation_host_->RegisterKeyframeEffectForElement(element_id,
                                                    keyframe_effect);
}

void AnimationPlayer::UnregisterKeyframeEffect(
    ElementId element_id,
    KeyframeEffectId keyframe_effect_id) {
  DCHECK(animation_host_);
  KeyframeEffect* keyframe_effect = GetKeyframeEffectById(keyframe_effect_id);
  DCHECK(keyframe_effect);
  if (keyframe_effect->has_attached_element() &&
      keyframe_effect->has_bound_element_animations()) {
    animation_host_->UnregisterKeyframeEffectForElement(element_id,
                                                        keyframe_effect);
  }
}
void AnimationPlayer::RegisterKeyframeEffects() {
  for (auto& element_id_keyframe_effect_id :
       element_to_keyframe_effect_id_map_) {
    const ElementId element_id = element_id_keyframe_effect_id.first;
    const std::unordered_set<KeyframeEffectId>& keyframe_effect_ids =
        element_id_keyframe_effect_id.second;
    for (auto& keyframe_effect_id : keyframe_effect_ids)
      RegisterKeyframeEffect(element_id, keyframe_effect_id);
  }
}

void AnimationPlayer::UnregisterKeyframeEffects() {
  for (auto& element_id_keyframe_effect_id :
       element_to_keyframe_effect_id_map_) {
    const ElementId element_id = element_id_keyframe_effect_id.first;
    const std::unordered_set<KeyframeEffectId>& keyframe_effect_ids =
        element_id_keyframe_effect_id.second;
    for (auto& keyframe_effect_id : keyframe_effect_ids)
      UnregisterKeyframeEffect(element_id, keyframe_effect_id);
  }
  animation_host_->RemoveFromTicking(this);
}

void AnimationPlayer::PushAttachedKeyframeEffectsToImplThread(
    AnimationPlayer* player_impl) const {
  for (auto& keyframe_effect : keyframe_effects_) {
    KeyframeEffect* keyframe_effect_impl =
        player_impl->GetKeyframeEffectById(keyframe_effect->id());
    if (keyframe_effect_impl)
      continue;

    std::unique_ptr<KeyframeEffect> to_add =
        keyframe_effect->CreateImplInstance();
    player_impl->AddKeyframeEffect(std::move(to_add));
  }
}

void AnimationPlayer::PushPropertiesToImplThread(AnimationPlayer* player_impl) {
  for (auto& keyframe_effect : keyframe_effects_) {
    if (KeyframeEffect* keyframe_effect_impl =
            player_impl->GetKeyframeEffectById(keyframe_effect->id())) {
      keyframe_effect->PushPropertiesTo(keyframe_effect_impl);
    }
  }
}

void AnimationPlayer::AddKeyframeModelForKeyframeEffect(
    std::unique_ptr<KeyframeModel> keyframe_model,
    KeyframeEffectId keyframe_effect_id) {
  DCHECK(GetKeyframeEffectById(keyframe_effect_id));
  GetKeyframeEffectById(keyframe_effect_id)
      ->AddKeyframeModel(std::move(keyframe_model));
}

void AnimationPlayer::PauseKeyframeModelForKeyframeEffect(
    int keyframe_model_id,
    double time_offset,
    KeyframeEffectId keyframe_effect_id) {
  DCHECK(GetKeyframeEffectById(keyframe_effect_id));
  GetKeyframeEffectById(keyframe_effect_id)
      ->PauseKeyframeModel(keyframe_model_id, time_offset);
}

void AnimationPlayer::RemoveKeyframeModelForKeyframeEffect(
    int keyframe_model_id,
    KeyframeEffectId keyframe_effect_id) {
  DCHECK(GetKeyframeEffectById(keyframe_effect_id));
  GetKeyframeEffectById(keyframe_effect_id)
      ->RemoveKeyframeModel(keyframe_model_id);
}

void AnimationPlayer::AbortKeyframeModelForKeyframeEffect(
    int keyframe_model_id,
    KeyframeEffectId keyframe_effect_id) {
  DCHECK(GetKeyframeEffectById(keyframe_effect_id));
  GetKeyframeEffectById(keyframe_effect_id)
      ->AbortKeyframeModel(keyframe_model_id);
}

void AnimationPlayer::AbortKeyframeModels(TargetProperty::Type target_property,
                                          bool needs_completion) {
  for (auto& keyframe_effect : keyframe_effects_)
    keyframe_effect->AbortKeyframeModels(target_property, needs_completion);
}

void AnimationPlayer::PushPropertiesTo(AnimationPlayer* player_impl) {
  PushAttachedKeyframeEffectsToImplThread(player_impl);
  PushPropertiesToImplThread(player_impl);
}

void AnimationPlayer::Tick(base::TimeTicks monotonic_time) {
  DCHECK(!monotonic_time.is_null());
  for (auto& keyframe_effect : keyframe_effects_)
    keyframe_effect->Tick(monotonic_time, nullptr);
}

void AnimationPlayer::UpdateState(bool start_ready_animations,
                                  AnimationEvents* events) {
  for (auto& keyframe_effect : keyframe_effects_) {
    keyframe_effect->UpdateState(start_ready_animations, events);
    keyframe_effect->UpdateTickingState(UpdateTickingType::NORMAL);
  }
}

void AnimationPlayer::AddToTicking() {
  ++ticking_keyframe_effects_count;
  if (ticking_keyframe_effects_count > 1)
    return;
  DCHECK(animation_host_);
  animation_host_->AddToTicking(this);
}

void AnimationPlayer::KeyframeModelRemovedFromTicking() {
  DCHECK_GE(ticking_keyframe_effects_count, 0);
  if (!ticking_keyframe_effects_count)
    return;
  --ticking_keyframe_effects_count;
  DCHECK(animation_host_);
  DCHECK_GE(ticking_keyframe_effects_count, 0);
  if (ticking_keyframe_effects_count)
    return;
  animation_host_->RemoveFromTicking(this);
}

void AnimationPlayer::NotifyKeyframeModelStarted(const AnimationEvent& event) {
  if (animation_delegate_) {
    animation_delegate_->NotifyAnimationStarted(
        event.monotonic_time, event.target_property, event.group_id);
  }
}

void AnimationPlayer::NotifyKeyframeModelFinished(const AnimationEvent& event) {
  if (animation_delegate_) {
    animation_delegate_->NotifyAnimationFinished(
        event.monotonic_time, event.target_property, event.group_id);
  }
}

void AnimationPlayer::NotifyKeyframeModelAborted(const AnimationEvent& event) {
  if (animation_delegate_) {
    animation_delegate_->NotifyAnimationAborted(
        event.monotonic_time, event.target_property, event.group_id);
  }
}

void AnimationPlayer::NotifyKeyframeModelTakeover(const AnimationEvent& event) {
  DCHECK(event.target_property == TargetProperty::SCROLL_OFFSET);

  if (animation_delegate_) {
    DCHECK(event.curve);
    std::unique_ptr<AnimationCurve> animation_curve = event.curve->Clone();
    animation_delegate_->NotifyAnimationTakeover(
        event.monotonic_time, event.target_property, event.animation_start_time,
        std::move(animation_curve));
  }
}

size_t AnimationPlayer::TickingKeyframeModelsCount() const {
  size_t count = 0;
  for (auto& keyframe_effect : keyframe_effects_)
    count += keyframe_effect->TickingKeyframeModelsCount();
  return count;
}

void AnimationPlayer::SetNeedsCommit() {
  DCHECK(animation_host_);
  animation_host_->SetNeedsCommit();
}

void AnimationPlayer::SetNeedsPushProperties() {
  if (!animation_timeline_)
    return;
  animation_timeline_->SetNeedsPushProperties();
}

void AnimationPlayer::ActivateKeyframeEffects() {
  for (auto& keyframe_effect : keyframe_effects_) {
    keyframe_effect->ActivateKeyframeEffects();
    keyframe_effect->UpdateTickingState(UpdateTickingType::NORMAL);
  }
}

KeyframeModel* AnimationPlayer::GetKeyframeModelForKeyframeEffect(
    TargetProperty::Type target_property,
    KeyframeEffectId keyframe_effect_id) const {
  DCHECK(GetKeyframeEffectById(keyframe_effect_id));
  return GetKeyframeEffectById(keyframe_effect_id)
      ->GetKeyframeModel(target_property);
}

std::string AnimationPlayer::ToString() const {
  std::string output = base::StringPrintf("AnimationPlayer{id=%d", id_);
  for (const auto& keyframe_effect : keyframe_effects_) {
    output +=
        base::StringPrintf(", element_id=%s, keyframe_models=[%s]",
                           keyframe_effect->element_id().ToString().c_str(),
                           keyframe_effect->KeyframeModelsToString().c_str());
  }
  return output + "}";
}

bool AnimationPlayer::IsWorkletAnimationPlayer() const {
  return false;
}

void AnimationPlayer::AddKeyframeEffect(
    std::unique_ptr<KeyframeEffect> keyframe_effect) {
  keyframe_effect->SetAnimationPlayer(this);
  keyframe_effects_.push_back(std::move(keyframe_effect));

  SetNeedsPushProperties();
}

KeyframeEffect* AnimationPlayer::GetKeyframeEffectById(
    KeyframeEffectId keyframe_effect_id) const {
  // May return nullptr when syncing keyframe_effects_ to impl.
  return keyframe_effects_.size() > keyframe_effect_id
             ? keyframe_effects_[keyframe_effect_id].get()
             : nullptr;
}

}  // namespace cc
