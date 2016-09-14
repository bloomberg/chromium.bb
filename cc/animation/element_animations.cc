// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/element_animations.h"

#include <stddef.h>

#include <algorithm>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/output/filter_operations.h"
#include "cc/trees/mutator_host_client.h"
#include "ui/gfx/geometry/box_f.h"

namespace cc {

scoped_refptr<ElementAnimations> ElementAnimations::Create() {
  return make_scoped_refptr(new ElementAnimations());
}

ElementAnimations::ElementAnimations()
    : players_list_(new PlayersList()),
      animation_host_(),
      element_id_(),
      is_active_(false),
      has_element_in_active_list_(false),
      has_element_in_pending_list_(false),
      scroll_offset_animation_was_interrupted_(false),
      needs_push_properties_(false) {
  ClearNeedsUpdateImplClientState();
}

ElementAnimations::~ElementAnimations() {}

void ElementAnimations::SetAnimationHost(AnimationHost* host) {
  animation_host_ = host;
}

void ElementAnimations::SetElementId(ElementId element_id) {
  element_id_ = element_id;
}

void ElementAnimations::InitAffectedElementTypes() {
  DCHECK(element_id_);
  DCHECK(animation_host_);

  UpdateActivation(ActivationType::FORCE);

  DCHECK(animation_host_->mutator_host_client());
  if (animation_host_->mutator_host_client()->IsElementInList(
          element_id_, ElementListType::ACTIVE)) {
    set_has_element_in_active_list(true);
  }
  if (animation_host_->mutator_host_client()->IsElementInList(
          element_id_, ElementListType::PENDING)) {
    set_has_element_in_pending_list(true);
  }
}

void ElementAnimations::ClearAffectedElementTypes() {
  DCHECK(animation_host_);

  if (has_element_in_active_list()) {
    IsAnimatingChanged(ElementListType::ACTIVE, TargetProperty::TRANSFORM,
                       AnimationChangeType::BOTH, false);
    IsAnimatingChanged(ElementListType::ACTIVE, TargetProperty::OPACITY,
                       AnimationChangeType::BOTH, false);
  }
  set_has_element_in_active_list(false);

  if (has_element_in_pending_list()) {
    IsAnimatingChanged(ElementListType::PENDING, TargetProperty::TRANSFORM,
                       AnimationChangeType::BOTH, false);
    IsAnimatingChanged(ElementListType::PENDING, TargetProperty::OPACITY,
                       AnimationChangeType::BOTH, false);
  }
  set_has_element_in_pending_list(false);

  animation_host_->DidDeactivateElementAnimations(this);
  UpdateActivation(ActivationType::FORCE);
}

void ElementAnimations::ElementRegistered(ElementId element_id,
                                          ElementListType list_type) {
  DCHECK_EQ(element_id_, element_id);

  if (!has_element_in_any_list())
    UpdateActivation(ActivationType::FORCE);

  if (list_type == ElementListType::ACTIVE)
    set_has_element_in_active_list(true);
  else
    set_has_element_in_pending_list(true);
}

void ElementAnimations::ElementUnregistered(ElementId element_id,
                                            ElementListType list_type) {
  DCHECK_EQ(this->element_id(), element_id);
  if (list_type == ElementListType::ACTIVE)
    set_has_element_in_active_list(false);
  else
    set_has_element_in_pending_list(false);

  if (!has_element_in_any_list())
    animation_host_->DidDeactivateElementAnimations(this);
}

void ElementAnimations::AddPlayer(AnimationPlayer* player) {
  players_list_->AddObserver(player);
}

void ElementAnimations::RemovePlayer(AnimationPlayer* player) {
  players_list_->RemoveObserver(player);
}

bool ElementAnimations::IsEmpty() const {
  return !players_list_->might_have_observers();
}

void ElementAnimations::SetNeedsPushProperties() {
  needs_push_properties_ = true;
}

void ElementAnimations::PushPropertiesTo(
    scoped_refptr<ElementAnimations> element_animations_impl) {
  DCHECK_NE(this, element_animations_impl);

  if (!needs_push_properties_)
    return;
  needs_push_properties_ = false;

  element_animations_impl->scroll_offset_animation_was_interrupted_ =
      scroll_offset_animation_was_interrupted_;
  scroll_offset_animation_was_interrupted_ = false;

  // Update impl client state.
  element_animations_impl->UpdateClientAnimationState(
      needs_update_impl_client_state_transform_,
      needs_update_impl_client_state_opacity_,
      needs_update_impl_client_state_filter_);
  ClearNeedsUpdateImplClientState();

  element_animations_impl->UpdateActivation(ActivationType::NORMAL);
  UpdateActivation(ActivationType::NORMAL);
}

void ElementAnimations::UpdateClientAnimationState(
    TargetProperty::Type target_property) {
  switch (target_property) {
    case TargetProperty::TRANSFORM:
    case TargetProperty::OPACITY:
    case TargetProperty::FILTER:
      UpdateClientAnimationStateInternal(target_property);
      break;
    default:
      // Do not update for other properties.
      break;
  }
}

void ElementAnimations::UpdateClientAnimationState(bool transform,
                                                   bool opacity,
                                                   bool filter) {
  if (transform)
    UpdateClientAnimationStateInternal(TargetProperty::TRANSFORM);
  if (opacity)
    UpdateClientAnimationStateInternal(TargetProperty::OPACITY);
  if (filter)
    UpdateClientAnimationStateInternal(TargetProperty::FILTER);
}

void ElementAnimations::AddAnimation(std::unique_ptr<Animation> animation) {
  // TODO(loyso): Erase this. Rewrite element_animations_unittest to use
  // AnimationPlayer::AddAnimation.

  // Add animation to the first player.
  DCHECK(players_list_->might_have_observers());
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player = it.GetNext();
  DCHECK(player);
  player->AddAnimation(std::move(animation));
}

void ElementAnimations::Animate(base::TimeTicks monotonic_time) {
  DCHECK(!monotonic_time.is_null());
  if (!has_element_in_active_list() && !has_element_in_pending_list())
    return;

  {
    // TODO(crbug.com/634916): Shouldn't manually iterate through the list if
    // base::ObserverList has a callback mechanism.
    ElementAnimations::PlayersList::Iterator it(players_list_.get());
    AnimationPlayer* player;
    while ((player = it.GetNext()) != nullptr) {
      if (player->needs_to_start_animations())
        player->StartAnimations(monotonic_time);
    }
  }
  {
    ElementAnimations::PlayersList::Iterator it(players_list_.get());
    AnimationPlayer* player;
    while ((player = it.GetNext()) != nullptr)
      player->TickAnimations(monotonic_time);
  }
  last_tick_time_ = monotonic_time;

  UpdateClientAnimationStateInternal(TargetProperty::OPACITY);
  UpdateClientAnimationStateInternal(TargetProperty::TRANSFORM);
  UpdateClientAnimationStateInternal(TargetProperty::FILTER);
}

void ElementAnimations::UpdateState(bool start_ready_animations,
                                    AnimationEvents* events) {
  if (!has_element_in_active_list())
    return;

  // Animate hasn't been called, this happens if an element has been added
  // between the Commit and Draw phases.
  if (last_tick_time_ == base::TimeTicks())
    return;

  if (start_ready_animations) {
    ElementAnimations::PlayersList::Iterator it(players_list_.get());
    AnimationPlayer* player;
    while ((player = it.GetNext()) != nullptr)
      player->PromoteStartedAnimations(last_tick_time_, events);
  }

  {
    ElementAnimations::PlayersList::Iterator it(players_list_.get());
    AnimationPlayer* player;
    while ((player = it.GetNext()) != nullptr)
      player->MarkFinishedAnimations(last_tick_time_);
  }
  {
    ElementAnimations::PlayersList::Iterator it(players_list_.get());
    AnimationPlayer* player;
    while ((player = it.GetNext()) != nullptr)
      player->MarkAnimationsForDeletion(last_tick_time_, events);
  }

  if (start_ready_animations) {
    ElementAnimations::PlayersList::Iterator it(players_list_.get());
    AnimationPlayer* player;
    while ((player = it.GetNext()) != nullptr) {
      if (player->needs_to_start_animations()) {
        player->StartAnimations(last_tick_time_);
        player->PromoteStartedAnimations(last_tick_time_, events);
      }
    }
  }

  UpdateActivation(ActivationType::NORMAL);
}

void ElementAnimations::ActivateAnimations() {
  bool changed_transform_animation = false;
  bool changed_opacity_animation = false;
  bool changed_filter_animation = false;

  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    player->ActivateAnimations(&changed_transform_animation,
                               &changed_opacity_animation,
                               &changed_filter_animation);
  }

  scroll_offset_animation_was_interrupted_ = false;
  UpdateActivation(ActivationType::NORMAL);
  UpdateClientAnimationState(changed_transform_animation,
                             changed_opacity_animation,
                             changed_filter_animation);
}

void ElementAnimations::NotifyAnimationStarted(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    if (player->NotifyAnimationStarted(event))
      break;
  }
}

void ElementAnimations::NotifyAnimationFinished(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    if (player->NotifyAnimationFinished(event))
      break;
  }
}

void ElementAnimations::NotifyAnimationTakeover(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
  DCHECK(event.target_property == TargetProperty::SCROLL_OFFSET);

  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr)
    player->NotifyAnimationTakeover(event);
}

void ElementAnimations::NotifyAnimationAborted(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);

  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    if (player->NotifyAnimationAborted(event))
      break;
  }

  UpdateClientAnimationState(event.target_property);
}

void ElementAnimations::NotifyAnimationPropertyUpdate(
    const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
  bool notify_active_elements = true;
  bool notify_pending_elements = true;
  switch (event.target_property) {
    case TargetProperty::OPACITY:
      NotifyClientOpacityAnimated(event.opacity, notify_active_elements,
                                  notify_pending_elements);
      break;
    case TargetProperty::TRANSFORM:
      NotifyClientTransformAnimated(event.transform, notify_active_elements,
                                    notify_pending_elements);
      break;
    default:
      NOTREACHED();
  }
}

bool ElementAnimations::HasFilterAnimationThatInflatesBounds() const {
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    if (player->HasFilterAnimationThatInflatesBounds())
      return true;
  }
  return false;
}

bool ElementAnimations::HasTransformAnimationThatInflatesBounds() const {
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    if (player->HasTransformAnimationThatInflatesBounds())
      return true;
  }
  return false;
}

bool ElementAnimations::FilterAnimationBoundsForBox(const gfx::BoxF& box,
                                                    gfx::BoxF* bounds) const {
  // TODO(avallee): Implement.
  return false;
}

bool ElementAnimations::TransformAnimationBoundsForBox(
    const gfx::BoxF& box,
    gfx::BoxF* bounds) const {
  *bounds = gfx::BoxF();

  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    gfx::BoxF player_bounds;
    bool success = player->TransformAnimationBoundsForBox(box, &player_bounds);
    if (!success)
      return false;
    bounds->Union(player_bounds);
  }
  return true;
}

bool ElementAnimations::HasAnimationThatAffectsScale() const {
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    if (player->HasAnimationThatAffectsScale())
      return true;
  }
  return false;
}

bool ElementAnimations::HasOnlyTranslationTransforms(
    ElementListType list_type) const {
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    if (!player->HasOnlyTranslationTransforms(list_type))
      return false;
  }
  return true;
}

bool ElementAnimations::AnimationsPreserveAxisAlignment() const {
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    if (!player->AnimationsPreserveAxisAlignment())
      return false;
  }
  return true;
}

bool ElementAnimations::AnimationStartScale(ElementListType list_type,
                                            float* start_scale) const {
  *start_scale = 0.f;

  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    float player_start_scale = 0.f;
    bool success = player->AnimationStartScale(list_type, &player_start_scale);
    if (!success)
      return false;
    // Union: a maximum.
    *start_scale = std::max(*start_scale, player_start_scale);
  }

  return true;
}

bool ElementAnimations::MaximumTargetScale(ElementListType list_type,
                                           float* max_scale) const {
  *max_scale = 0.f;

  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    float player_max_scale = 0.f;
    bool success = player->MaximumTargetScale(list_type, &player_max_scale);
    if (!success)
      return false;
    // Union: a maximum.
    *max_scale = std::max(*max_scale, player_max_scale);
  }

  return true;
}

void ElementAnimations::SetNeedsUpdateImplClientState(bool transform,
                                                      bool opacity,
                                                      bool filter) {
  if (transform)
    needs_update_impl_client_state_transform_ = true;
  if (opacity)
    needs_update_impl_client_state_opacity_ = true;
  if (filter)
    needs_update_impl_client_state_filter_ = true;

  if (needs_update_impl_client_state_transform_ ||
      needs_update_impl_client_state_opacity_ ||
      needs_update_impl_client_state_filter_)
    SetNeedsPushProperties();
}

void ElementAnimations::ClearNeedsUpdateImplClientState() {
  needs_update_impl_client_state_transform_ = false;
  needs_update_impl_client_state_opacity_ = false;
  needs_update_impl_client_state_filter_ = false;
}

void ElementAnimations::UpdateActivation(ActivationType type) {
  bool force = type == ActivationType::FORCE;
  if (animation_host_) {
    bool was_active = is_active_;
    is_active_ = false;

    ElementAnimations::PlayersList::Iterator it(players_list_.get());
    AnimationPlayer* player;
    while ((player = it.GetNext()) != nullptr) {
      if (player->HasNonDeletedAnimation()) {
        is_active_ = true;
        break;
      }
    }

    if (is_active_ && (!was_active || force)) {
      animation_host_->DidActivateElementAnimations(this);
    } else if (!is_active_ && (was_active || force)) {
      // Resetting last_tick_time_ here ensures that calling ::UpdateState
      // before ::Animate doesn't start an animation.
      last_tick_time_ = base::TimeTicks();
      animation_host_->DidDeactivateElementAnimations(this);
    }
  }
}

void ElementAnimations::UpdateActivationNormal() {
  UpdateActivation(ActivationType::NORMAL);
}

void ElementAnimations::NotifyClientOpacityAnimated(
    float opacity,
    bool notify_active_elements,
    bool notify_pending_elements) {
  if (notify_active_elements && has_element_in_active_list())
    OnOpacityAnimated(ElementListType::ACTIVE, opacity);
  if (notify_pending_elements && has_element_in_pending_list())
    OnOpacityAnimated(ElementListType::PENDING, opacity);
}

void ElementAnimations::NotifyClientTransformAnimated(
    const gfx::Transform& transform,
    bool notify_active_elements,
    bool notify_pending_elements) {
  if (notify_active_elements && has_element_in_active_list())
    OnTransformAnimated(ElementListType::ACTIVE, transform);
  if (notify_pending_elements && has_element_in_pending_list())
    OnTransformAnimated(ElementListType::PENDING, transform);
}

void ElementAnimations::NotifyClientFilterAnimated(
    const FilterOperations& filters,
    bool notify_active_elements,
    bool notify_pending_elements) {
  if (notify_active_elements && has_element_in_active_list())
    OnFilterAnimated(ElementListType::ACTIVE, filters);
  if (notify_pending_elements && has_element_in_pending_list())
    OnFilterAnimated(ElementListType::PENDING, filters);
}

void ElementAnimations::NotifyClientScrollOffsetAnimated(
    const gfx::ScrollOffset& scroll_offset,
    bool notify_active_elements,
    bool notify_pending_elements) {
  if (notify_active_elements && has_element_in_active_list())
    OnScrollOffsetAnimated(ElementListType::ACTIVE, scroll_offset);
  if (notify_pending_elements && has_element_in_pending_list())
    OnScrollOffsetAnimated(ElementListType::PENDING, scroll_offset);
}

void ElementAnimations::NotifyClientAnimationChanged(
    TargetProperty::Type property,
    ElementListType list_type,
    bool notify_elements_about_potential_animation,
    bool notify_elements_about_running_animation) {
  struct PropertyAnimationState* animation_state = nullptr;
  switch (property) {
    case TargetProperty::OPACITY:
      animation_state = &opacity_animation_state_;
      break;
    case TargetProperty::TRANSFORM:
      animation_state = &transform_animation_state_;
      break;
    case TargetProperty::FILTER:
      animation_state = &filter_animation_state_;
      break;
    default:
      NOTREACHED();
      break;
  }

  bool notify_elements_about_potential_and_running_animation =
      notify_elements_about_potential_animation &&
      notify_elements_about_running_animation;
  bool active = list_type == ElementListType::ACTIVE;
  if (notify_elements_about_potential_and_running_animation) {
    bool potentially_animating =
        active ? animation_state->potentially_animating_for_active_elements
               : animation_state->potentially_animating_for_pending_elements;
    bool currently_animating =
        active ? animation_state->currently_running_for_active_elements
               : animation_state->currently_running_for_pending_elements;
    DCHECK_EQ(potentially_animating, currently_animating);
    IsAnimatingChanged(list_type, property, AnimationChangeType::BOTH,
                       potentially_animating);
  } else if (notify_elements_about_potential_animation) {
    bool potentially_animating =
        active ? animation_state->potentially_animating_for_active_elements
               : animation_state->potentially_animating_for_pending_elements;
    IsAnimatingChanged(list_type, property, AnimationChangeType::POTENTIAL,
                       potentially_animating);
  } else if (notify_elements_about_running_animation) {
    bool currently_animating =
        active ? animation_state->currently_running_for_active_elements
               : animation_state->currently_running_for_pending_elements;
    IsAnimatingChanged(list_type, property, AnimationChangeType::RUNNING,
                       currently_animating);
  }
}

void ElementAnimations::UpdateClientAnimationStateInternal(
    TargetProperty::Type property) {
  struct PropertyAnimationState* animation_state = nullptr;
  switch (property) {
    case TargetProperty::OPACITY:
      animation_state = &opacity_animation_state_;
      break;
    case TargetProperty::TRANSFORM:
      animation_state = &transform_animation_state_;
      break;
    case TargetProperty::FILTER:
      animation_state = &filter_animation_state_;
      break;
    default:
      NOTREACHED();
      break;
  }
  bool was_currently_running_animation_for_active_elements =
      animation_state->currently_running_for_active_elements;
  bool was_currently_running_animation_for_pending_elements =
      animation_state->currently_running_for_pending_elements;
  bool was_potentially_animating_for_active_elements =
      animation_state->potentially_animating_for_active_elements;
  bool was_potentially_animating_for_pending_elements =
      animation_state->potentially_animating_for_pending_elements;

  animation_state->Clear();
  DCHECK(was_potentially_animating_for_active_elements ||
         !was_currently_running_animation_for_active_elements);
  DCHECK(was_potentially_animating_for_pending_elements ||
         !was_currently_running_animation_for_pending_elements);

  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    for (const auto& animation : player->animations()) {
      if (!animation->is_finished() &&
          animation->target_property() == property) {
        animation_state->potentially_animating_for_active_elements |=
            animation->affects_active_elements();
        animation_state->potentially_animating_for_pending_elements |=
            animation->affects_pending_elements();
        animation_state->currently_running_for_active_elements =
            animation_state->potentially_animating_for_active_elements &&
            animation->InEffect(last_tick_time_);
        animation_state->currently_running_for_pending_elements =
            animation_state->potentially_animating_for_pending_elements &&
            animation->InEffect(last_tick_time_);
      }
    }
  }

  bool potentially_animating_changed_for_active_elements =
      was_potentially_animating_for_active_elements !=
      animation_state->potentially_animating_for_active_elements;
  bool potentially_animating_changed_for_pending_elements =
      was_potentially_animating_for_pending_elements !=
      animation_state->potentially_animating_for_pending_elements;
  bool currently_running_animation_changed_for_active_elements =
      was_currently_running_animation_for_active_elements !=
      animation_state->currently_running_for_active_elements;
  bool currently_running_animation_changed_for_pending_elements =
      was_currently_running_animation_for_pending_elements !=
      animation_state->currently_running_for_pending_elements;
  if (!potentially_animating_changed_for_active_elements &&
      !potentially_animating_changed_for_pending_elements &&
      !currently_running_animation_changed_for_active_elements &&
      !currently_running_animation_changed_for_pending_elements)
    return;
  if (has_element_in_active_list())
    NotifyClientAnimationChanged(
        property, ElementListType::ACTIVE,
        potentially_animating_changed_for_active_elements,
        currently_running_animation_changed_for_active_elements);
  if (has_element_in_pending_list())
    NotifyClientAnimationChanged(
        property, ElementListType::PENDING,
        potentially_animating_changed_for_pending_elements,
        currently_running_animation_changed_for_pending_elements);
}

bool ElementAnimations::HasActiveAnimation() const {
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    if (player->HasActiveAnimation())
      return true;
  }

  return false;
}

bool ElementAnimations::HasAnyAnimation() const {
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    if (player->has_any_animation())
      return true;
  }

  return false;
}

bool ElementAnimations::IsPotentiallyAnimatingProperty(
    TargetProperty::Type target_property,
    ElementListType list_type) const {
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    if (player->IsPotentiallyAnimatingProperty(target_property, list_type))
      return true;
  }

  return false;
}

bool ElementAnimations::IsCurrentlyAnimatingProperty(
    TargetProperty::Type target_property,
    ElementListType list_type) const {
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    if (player->IsCurrentlyAnimatingProperty(target_property, list_type))
      return true;
  }

  return false;
}

void ElementAnimations::SetScrollOffsetAnimationWasInterrupted() {
  scroll_offset_animation_was_interrupted_ = true;
}

bool ElementAnimations::needs_to_start_animations_for_testing() const {
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    if (player->needs_to_start_animations())
      return true;
  }

  return false;
}

void ElementAnimations::PauseAnimation(int animation_id,
                                       base::TimeDelta time_offset) {
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr)
    player->PauseAnimation(animation_id, time_offset.InSecondsF());
}

void ElementAnimations::RemoveAnimation(int animation_id) {
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr)
    player->RemoveAnimation(animation_id);
}

void ElementAnimations::AbortAnimation(int animation_id) {
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr)
    player->AbortAnimation(animation_id);
}

void ElementAnimations::AbortAnimations(TargetProperty::Type target_property,
                                        bool needs_completion) {
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr)
    player->AbortAnimations(target_property, needs_completion);
}

Animation* ElementAnimations::GetAnimation(
    TargetProperty::Type target_property) const {
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    if (Animation* animation = player->GetAnimation(target_property))
      return animation;
  }
  return nullptr;
}

Animation* ElementAnimations::GetAnimationById(int animation_id) const {
  ElementAnimations::PlayersList::Iterator it(players_list_.get());
  AnimationPlayer* player;
  while ((player = it.GetNext()) != nullptr) {
    if (Animation* animation = player->GetAnimationById(animation_id))
      return animation;
  }
  return nullptr;
}

void ElementAnimations::OnFilterAnimated(ElementListType list_type,
                                         const FilterOperations& filters) {
  DCHECK(element_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetElementFilterMutated(
      element_id(), list_type, filters);
}

void ElementAnimations::OnOpacityAnimated(ElementListType list_type,
                                          float opacity) {
  DCHECK(element_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetElementOpacityMutated(
      element_id(), list_type, opacity);
}

void ElementAnimations::OnTransformAnimated(ElementListType list_type,
                                            const gfx::Transform& transform) {
  DCHECK(element_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetElementTransformMutated(
      element_id(), list_type, transform);
}

void ElementAnimations::OnScrollOffsetAnimated(
    ElementListType list_type,
    const gfx::ScrollOffset& scroll_offset) {
  DCHECK(element_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetElementScrollOffsetMutated(
      element_id(), list_type, scroll_offset);
}

void ElementAnimations::IsAnimatingChanged(ElementListType list_type,
                                           TargetProperty::Type property,
                                           AnimationChangeType change_type,
                                           bool is_animating) {
  if (!element_id())
    return;
  DCHECK(animation_host());
  if (animation_host()->mutator_host_client()) {
    switch (property) {
      case TargetProperty::OPACITY:
        animation_host()
            ->mutator_host_client()
            ->ElementOpacityIsAnimatingChanged(element_id(), list_type,
                                               change_type, is_animating);
        break;
      case TargetProperty::TRANSFORM:
        animation_host()
            ->mutator_host_client()
            ->ElementTransformIsAnimatingChanged(element_id(), list_type,
                                                 change_type, is_animating);
        break;
      case TargetProperty::FILTER:
        animation_host()
            ->mutator_host_client()
            ->ElementFilterIsAnimatingChanged(element_id(), list_type,
                                              change_type, is_animating);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

gfx::ScrollOffset ElementAnimations::ScrollOffsetForAnimation() const {
  if (animation_host()) {
    DCHECK(animation_host()->mutator_host_client());
    return animation_host()->mutator_host_client()->GetScrollOffsetForAnimation(
        element_id());
  }

  return gfx::ScrollOffset();
}

}  // namespace cc
