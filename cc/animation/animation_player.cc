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
#include "cc/animation/animation_ticker.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/transform_operations.h"
#include "cc/trees/property_animation_state.h"

namespace cc {

scoped_refptr<AnimationPlayer> AnimationPlayer::Create(int id) {
  return make_scoped_refptr(new AnimationPlayer(id));
}

AnimationPlayer::AnimationPlayer(int id)
    : animation_host_(),
      animation_timeline_(),
      animation_delegate_(),
      id_(id),
      needs_push_properties_(false),
      animation_ticker_(new AnimationTicker(this)) {
  DCHECK(id_);
}

AnimationPlayer::~AnimationPlayer() {
  DCHECK(!animation_timeline_);
}

scoped_refptr<AnimationPlayer> AnimationPlayer::CreateImplInstance() const {
  scoped_refptr<AnimationPlayer> player = AnimationPlayer::Create(id());
  return player;
}

ElementId AnimationPlayer::element_id() const {
  return animation_ticker_->element_id();
}

void AnimationPlayer::SetAnimationHost(AnimationHost* animation_host) {
  animation_host_ = animation_host;
}

void AnimationPlayer::SetAnimationTimeline(AnimationTimeline* timeline) {
  if (animation_timeline_ == timeline)
    return;

  // We need to unregister player to manage ElementAnimations and observers
  // properly.
  if (animation_ticker_->has_attached_element() &&
      animation_ticker_->has_bound_element_animations())
    UnregisterPlayer();

  animation_timeline_ = timeline;

  // Register player only if layer AND host attached.
  if (animation_ticker_->has_attached_element() && animation_host_)
    RegisterPlayer();
}

scoped_refptr<ElementAnimations> AnimationPlayer::element_animations() const {
  return animation_ticker_->element_animations();
}

void AnimationPlayer::AttachElement(ElementId element_id) {
  animation_ticker_->AttachElement(element_id);

  // Register player only if layer AND host attached.
  if (animation_host_)
    RegisterPlayer();
}

void AnimationPlayer::DetachElement() {
  DCHECK(animation_ticker_->has_attached_element());

  if (animation_host_)
    UnregisterPlayer();

  animation_ticker_->DetachElement();
}

void AnimationPlayer::RegisterPlayer() {
  DCHECK(animation_host_);
  DCHECK(animation_ticker_->has_attached_element());
  DCHECK(!animation_ticker_->has_bound_element_animations());

  // Create ElementAnimations or re-use existing.
  animation_host_->RegisterPlayerForElement(animation_ticker_->element_id(),
                                            this);
  // Get local reference to shared ElementAnimations.
  BindElementAnimations();
}

void AnimationPlayer::UnregisterPlayer() {
  DCHECK(animation_host_);
  DCHECK(animation_ticker_->has_attached_element());
  DCHECK(animation_ticker_->has_bound_element_animations());

  UnbindElementAnimations();
  // Destroy ElementAnimations or release it if it's still needed.
  animation_host_->UnregisterPlayerForElement(animation_ticker_->element_id(),
                                              this);
}

void AnimationPlayer::BindElementAnimations() {
  animation_ticker_->BindElementAnimations(animation_host_);
  SetNeedsPushProperties();
}

void AnimationPlayer::UnbindElementAnimations() {
  SetNeedsPushProperties();
  animation_ticker_->UnbindElementAnimations();
}

void AnimationPlayer::AddAnimation(std::unique_ptr<Animation> animation) {
  animation_ticker_->AddAnimation(std::move(animation));
}

void AnimationPlayer::PauseAnimation(int animation_id, double time_offset) {
  animation_ticker_->PauseAnimation(animation_id, time_offset);
}

void AnimationPlayer::RemoveAnimation(int animation_id) {
  animation_ticker_->RemoveAnimation(animation_id);
}

void AnimationPlayer::AbortAnimation(int animation_id) {
  animation_ticker_->AbortAnimation(animation_id);
}

void AnimationPlayer::AbortAnimations(TargetProperty::Type target_property,
                                      bool needs_completion) {
  animation_ticker_->AbortAnimations(target_property, needs_completion);
}

void AnimationPlayer::PushPropertiesTo(AnimationPlayer* player_impl) {
  if (!needs_push_properties_)
    return;
  needs_push_properties_ = false;

  // Create or destroy ElementAnimations.
  ElementId element_id = animation_ticker_->element_id();
  AnimationTicker* animation_ticker_impl = player_impl->animation_ticker_.get();
  if (element_id != animation_ticker_impl->element_id()) {
    if (animation_ticker_impl->has_attached_element())
      player_impl->DetachElement();
    if (element_id)
      player_impl->AttachElement(element_id);
  }

  if (!animation_ticker_->has_any_animation() &&
      !animation_ticker_impl->has_any_animation())
    return;

  animation_ticker_->MarkAbortedAnimationsForDeletion(animation_ticker_impl);
  animation_ticker_->PurgeAnimationsMarkedForDeletion(/* impl_only */ false);
  animation_ticker_->PushNewAnimationsToImplThread(animation_ticker_impl);

  // Remove finished impl side animations only after pushing,
  // and only after the animations are deleted on the main thread
  // this insures we will never push an animation twice.
  animation_ticker_->RemoveAnimationsCompletedOnMainThread(
      animation_ticker_impl);

  PushPropertiesToImplThread(player_impl);

  player_impl->UpdateTickingState(UpdateTickingType::NORMAL);
}

void AnimationPlayer::Tick(base::TimeTicks monotonic_time) {
  DCHECK(!monotonic_time.is_null());
  animation_ticker_->Tick(monotonic_time);
}

void AnimationPlayer::UpdateState(bool start_ready_animations,
                                  AnimationEvents* events) {
  animation_ticker_->UpdateState(start_ready_animations, events);

  UpdateTickingState(UpdateTickingType::NORMAL);
}

void AnimationPlayer::UpdateTickingState(UpdateTickingType type) {
  animation_ticker_->UpdateTickingState(type);
}

void AnimationPlayer::AddToTicking() {
  DCHECK(animation_host_);
  animation_host_->AddToTicking(this);
}

void AnimationPlayer::RemoveFromTicking() {
  DCHECK(animation_host_);
  // Resetting last_tick_time_ here ensures that calling ::UpdateState
  // before ::Animate doesn't start an animation.
  animation_ticker_->RemoveFromTicking();
  animation_host_->RemoveFromTicking(this);
}

bool AnimationPlayer::NotifyAnimationStarted(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);

  if (animation_ticker_->NotifyAnimationStarted(event)) {
    if (animation_delegate_) {
      animation_delegate_->NotifyAnimationStarted(
          event.monotonic_time, event.target_property, event.group_id);
    }
    return true;
  }
  return false;
}

bool AnimationPlayer::NotifyAnimationFinished(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
  if (animation_ticker_->NotifyAnimationFinished(event)) {
    if (animation_delegate_) {
      animation_delegate_->NotifyAnimationFinished(
          event.monotonic_time, event.target_property, event.group_id);
    }
    return true;
  }

  // This is for the case when an animation is already removed on main thread,
  // but the impl version of it sent a finished event and is now waiting for
  // deletion. We would need to delete that animation during push properties.
  SetNeedsPushProperties();
  return false;
}

bool AnimationPlayer::NotifyAnimationFinishedForTesting(
    TargetProperty::Type target_property,
    int group_id) {
  AnimationEvent event(AnimationEvent::FINISHED,
                       animation_ticker_->element_id(), group_id,
                       target_property, base::TimeTicks());
  return NotifyAnimationFinished(event);
}

bool AnimationPlayer::NotifyAnimationAborted(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
  if (animation_ticker_->NotifyAnimationAborted(event)) {
    if (animation_delegate_) {
      animation_delegate_->NotifyAnimationAborted(
          event.monotonic_time, event.target_property, event.group_id);
    }
    return true;
  }
  return false;
}

void AnimationPlayer::NotifyAnimationTakeover(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
  DCHECK(event.target_property == TargetProperty::SCROLL_OFFSET);

  // We need to purge animations marked for deletion on CT.
  SetNeedsPushProperties();

  if (animation_delegate_) {
    DCHECK(event.curve);
    std::unique_ptr<AnimationCurve> animation_curve = event.curve->Clone();
    animation_delegate_->NotifyAnimationTakeover(
        event.monotonic_time, event.target_property, event.animation_start_time,
        std::move(animation_curve));
  }
}

void AnimationPlayer::NotifyImplOnlyAnimationStarted(
    AnimationEvent& started_event) {
  if (animation_delegate_) {
    animation_delegate_->NotifyAnimationStarted(started_event.monotonic_time,
                                                started_event.target_property,
                                                started_event.group_id);
  }
}

void AnimationPlayer::NotifyImplOnlyAnimationFinished(
    AnimationEvent& finished_event) {
  if (animation_delegate_) {
    animation_delegate_->NotifyAnimationFinished(finished_event.monotonic_time,
                                                 finished_event.target_property,
                                                 finished_event.group_id);
  }
}

void AnimationPlayer::NotifyAnimationTakeoverByMain(
    AnimationEvent& aborted_event) {
  if (animation_delegate_) {
    animation_delegate_->NotifyAnimationFinished(aborted_event.monotonic_time,
                                                 aborted_event.target_property,
                                                 aborted_event.group_id);
  }
}

void AnimationPlayer::SetNeedsCommit() {
  DCHECK(animation_host_);
  animation_host_->SetNeedsCommit();
}

void AnimationPlayer::SetNeedsPushProperties() {
  needs_push_properties_ = true;

  DCHECK(animation_timeline_);
  animation_timeline_->SetNeedsPushProperties();

  DCHECK(element_animations());
  element_animations()->SetNeedsPushProperties();
}

bool AnimationPlayer::HasTickingAnimation() const {
  return animation_ticker_->HasTickingAnimation();
}

bool AnimationPlayer::has_any_animation() const {
  return animation_ticker_->has_any_animation();
}

void AnimationPlayer::ActivateAnimations() {
  animation_ticker_->ActivateAnimations();
  UpdateTickingState(UpdateTickingType::NORMAL);
}

bool AnimationPlayer::HasOnlyTranslationTransforms(
    ElementListType list_type) const {
  return animation_ticker_->HasOnlyTranslationTransforms(list_type);
}

bool AnimationPlayer::AnimationsPreserveAxisAlignment() const {
  return animation_ticker_->AnimationsPreserveAxisAlignment();
}

bool AnimationPlayer::AnimationStartScale(ElementListType list_type,
                                          float* start_scale) const {
  return animation_ticker_->AnimationStartScale(list_type, start_scale);
}

bool AnimationPlayer::MaximumTargetScale(ElementListType list_type,
                                         float* max_scale) const {
  return animation_ticker_->MaximumTargetScale(list_type, max_scale);
}

bool AnimationPlayer::IsPotentiallyAnimatingProperty(
    TargetProperty::Type target_property,
    ElementListType list_type) const {
  return animation_ticker_->IsPotentiallyAnimatingProperty(target_property,
                                                           list_type);
}

bool AnimationPlayer::IsCurrentlyAnimatingProperty(
    TargetProperty::Type target_property,
    ElementListType list_type) const {
  return animation_ticker_->IsCurrentlyAnimatingProperty(target_property,
                                                         list_type);
}

Animation* AnimationPlayer::GetAnimation(
    TargetProperty::Type target_property) const {
  return animation_ticker_->GetAnimation(target_property);
}

Animation* AnimationPlayer::GetAnimationById(int animation_id) const {
  return animation_ticker_->GetAnimationById(animation_id);
}

void AnimationPlayer::GetPropertyAnimationState(
    PropertyAnimationState* pending_state,
    PropertyAnimationState* active_state) const {
  animation_ticker_->GetPropertyAnimationState(pending_state, active_state);
}

void AnimationPlayer::PushPropertiesToImplThread(
    AnimationPlayer* animation_player_impl) {
  animation_ticker_->PushPropertiesToImplThread(
      animation_player_impl->animation_ticker_.get());
}

bool AnimationPlayer::scroll_offset_animation_was_interrupted() const {
  return animation_ticker_->scroll_offset_animation_was_interrupted();
}

std::string AnimationPlayer::ToString() const {
  return base::StringPrintf(
      "AnimationPlayer{id=%d, element_id=%s, animations=[%s]}", id_,
      animation_ticker_->element_id().ToString().c_str(),
      animation_ticker_->AnimationsToString().c_str());
}

}  // namespace cc
