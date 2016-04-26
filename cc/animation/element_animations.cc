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
#include "cc/animation/scroll_offset_animation_curve.h"
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
      layer_id_(),
      is_active_(false),
      needs_active_value_observations_(false),
      needs_pending_value_observations_(false),
      needs_to_start_animations_(false),
      scroll_offset_animation_was_interrupted_(false),
      potentially_animating_transform_for_active_observers_(false),
      potentially_animating_transform_for_pending_observers_(false) {}

ElementAnimations::~ElementAnimations() {}

void ElementAnimations::SetAnimationHost(AnimationHost* host) {
  animation_host_ = host;
}

void ElementAnimations::SetLayerId(int layer_id) {
  layer_id_ = layer_id;
}

void ElementAnimations::InitValueObservations() {
  DCHECK(layer_id_);
  DCHECK(animation_host_);

  UpdateActivation(FORCE_ACTIVATION);

  DCHECK(animation_host_->mutator_host_client());
  if (animation_host_->mutator_host_client()->IsLayerInTree(
          layer_id_, LayerTreeType::ACTIVE)) {
    set_needs_active_value_observations(true);
  }
  if (animation_host_->mutator_host_client()->IsLayerInTree(
          layer_id_, LayerTreeType::PENDING)) {
    set_needs_pending_value_observations(true);
  }
}

void ElementAnimations::ClearValueObservations() {
  DCHECK(animation_host_);

  if (needs_active_value_observations())
    OnTransformIsPotentiallyAnimatingChanged(LayerTreeType::ACTIVE, false);
  set_needs_active_value_observations(false);

  if (needs_pending_value_observations())
    OnTransformIsPotentiallyAnimatingChanged(LayerTreeType::PENDING, false);
  set_needs_pending_value_observations(false);

  animation_host_->DidDeactivateElementAnimations(this);
  UpdateActivation(FORCE_ACTIVATION);
}

void ElementAnimations::LayerRegistered(int layer_id, LayerTreeType tree_type) {
  DCHECK_EQ(layer_id_, layer_id);

  if (tree_type == LayerTreeType::ACTIVE)
    set_needs_active_value_observations(true);
  else
    set_needs_pending_value_observations(true);
}

void ElementAnimations::LayerUnregistered(int layer_id,
                                          LayerTreeType tree_type) {
  DCHECK_EQ(this->layer_id(), layer_id);
  if (tree_type == LayerTreeType::ACTIVE)
    set_needs_active_value_observations(false);
  else
    set_needs_pending_value_observations(false);
}

void ElementAnimations::AddPlayer(AnimationPlayer* player) {
  players_list_->Append(player);
}

void ElementAnimations::RemovePlayer(AnimationPlayer* player) {
  for (PlayersListNode* node = players_list_->head();
       node != players_list_->end(); node = node->next()) {
    if (node->value() == player) {
      node->RemoveFromList();
      return;
    }
  }
}

bool ElementAnimations::IsEmpty() const {
  return players_list_->empty();
}

void ElementAnimations::PushPropertiesTo(
    scoped_refptr<ElementAnimations> element_animations_impl) {
  DCHECK_NE(this, element_animations_impl);
  if (!has_any_animation() && !element_animations_impl->has_any_animation())
    return;
  MarkAbortedAnimationsForDeletion(element_animations_impl.get());
  PurgeAnimationsMarkedForDeletion();
  PushNewAnimationsToImplThread(element_animations_impl.get());

  // Remove finished impl side animations only after pushing,
  // and only after the animations are deleted on the main thread
  // this insures we will never push an animation twice.
  RemoveAnimationsCompletedOnMainThread(element_animations_impl.get());

  PushPropertiesToImplThread(element_animations_impl.get());
  element_animations_impl->UpdateActivation(NORMAL_ACTIVATION);
  UpdateActivation(NORMAL_ACTIVATION);
}

void ElementAnimations::AddAnimation(std::unique_ptr<Animation> animation) {
  bool added_transform_animation =
      animation->target_property() == TargetProperty::TRANSFORM;
  animations_.push_back(std::move(animation));
  needs_to_start_animations_ = true;
  UpdateActivation(NORMAL_ACTIVATION);
  if (added_transform_animation)
    UpdatePotentiallyAnimatingTransform();
}

void ElementAnimations::Animate(base::TimeTicks monotonic_time) {
  DCHECK(!monotonic_time.is_null());
  if (!needs_active_value_observations() && !needs_pending_value_observations())
    return;

  if (needs_to_start_animations_)
    StartAnimations(monotonic_time);
  TickAnimations(monotonic_time);
  last_tick_time_ = monotonic_time;
}

void ElementAnimations::AccumulatePropertyUpdates(
    base::TimeTicks monotonic_time,
    AnimationEvents* events) {
  if (!events)
    return;

  for (size_t i = 0; i < animations_.size(); ++i) {
    Animation* animation = animations_[i].get();
    if (!animation->is_impl_only())
      continue;

    if (!animation->InEffect(monotonic_time))
      continue;

    base::TimeDelta trimmed =
        animation->TrimTimeToCurrentIteration(monotonic_time);
    switch (animation->target_property()) {
      case TargetProperty::OPACITY: {
        AnimationEvent event(AnimationEvent::PROPERTY_UPDATE, layer_id_,
                             animation->group(), TargetProperty::OPACITY,
                             monotonic_time);
        const FloatAnimationCurve* float_animation_curve =
            animation->curve()->ToFloatAnimationCurve();
        event.opacity = float_animation_curve->GetValue(trimmed);
        event.is_impl_only = true;
        events->events_.push_back(event);
        break;
      }

      case TargetProperty::TRANSFORM: {
        AnimationEvent event(AnimationEvent::PROPERTY_UPDATE, layer_id_,
                             animation->group(), TargetProperty::TRANSFORM,
                             monotonic_time);
        const TransformAnimationCurve* transform_animation_curve =
            animation->curve()->ToTransformAnimationCurve();
        event.transform = transform_animation_curve->GetValue(trimmed);
        event.is_impl_only = true;
        events->events_.push_back(event);
        break;
      }

      case TargetProperty::FILTER: {
        AnimationEvent event(AnimationEvent::PROPERTY_UPDATE, layer_id_,
                             animation->group(), TargetProperty::FILTER,
                             monotonic_time);
        const FilterAnimationCurve* filter_animation_curve =
            animation->curve()->ToFilterAnimationCurve();
        event.filters = filter_animation_curve->GetValue(trimmed);
        event.is_impl_only = true;
        events->events_.push_back(event);
        break;
      }

      case TargetProperty::BACKGROUND_COLOR: {
        break;
      }

      case TargetProperty::SCROLL_OFFSET: {
        // Impl-side changes to scroll offset are already sent back to the
        // main thread (e.g. for user-driven scrolling), so a PROPERTY_UPDATE
        // isn't needed.
        break;
      }
    }
  }
}

void ElementAnimations::UpdateState(bool start_ready_animations,
                                    AnimationEvents* events) {
  if (!needs_active_value_observations())
    return;

  // Animate hasn't been called, this happens if an observer has been added
  // between the Commit and Draw phases.
  if (last_tick_time_ == base::TimeTicks())
    return;

  if (start_ready_animations)
    PromoteStartedAnimations(last_tick_time_, events);

  MarkFinishedAnimations(last_tick_time_);
  MarkAnimationsForDeletion(last_tick_time_, events);

  if (needs_to_start_animations_ && start_ready_animations) {
    StartAnimations(last_tick_time_);
    PromoteStartedAnimations(last_tick_time_, events);
  }

  AccumulatePropertyUpdates(last_tick_time_, events);

  UpdateActivation(NORMAL_ACTIVATION);
}

void ElementAnimations::ActivateAnimations() {
  bool changed_transform_animation = false;
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->affects_active_observers() !=
            animations_[i]->affects_pending_observers() &&
        animations_[i]->target_property() == TargetProperty::TRANSFORM)
      changed_transform_animation = true;
    animations_[i]->set_affects_active_observers(
        animations_[i]->affects_pending_observers());
  }
  auto affects_no_observers = [](const std::unique_ptr<Animation>& animation) {
    return !animation->affects_active_observers() &&
           !animation->affects_pending_observers();
  };
  animations_.erase(std::remove_if(animations_.begin(), animations_.end(),
                                   affects_no_observers),
                    animations_.end());
  scroll_offset_animation_was_interrupted_ = false;
  UpdateActivation(NORMAL_ACTIVATION);
  if (changed_transform_animation)
    UpdatePotentiallyAnimatingTransform();
}

void ElementAnimations::NotifyAnimationStarted(const AnimationEvent& event) {
  if (event.is_impl_only) {
    NotifyPlayersAnimationStarted(event.monotonic_time, event.target_property,
                                  event.group_id);
    return;
  }

  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->group() == event.group_id &&
        animations_[i]->target_property() == event.target_property &&
        animations_[i]->needs_synchronized_start_time()) {
      animations_[i]->set_needs_synchronized_start_time(false);
      if (!animations_[i]->has_set_start_time())
        animations_[i]->set_start_time(event.monotonic_time);

      NotifyPlayersAnimationStarted(event.monotonic_time, event.target_property,
                                    event.group_id);

      return;
    }
  }
}

void ElementAnimations::NotifyAnimationFinished(const AnimationEvent& event) {
  if (event.is_impl_only) {
    NotifyPlayersAnimationFinished(event.monotonic_time, event.target_property,
                                   event.group_id);
    return;
  }

  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->group() == event.group_id &&
        animations_[i]->target_property() == event.target_property) {
      animations_[i]->set_received_finished_event(true);
      NotifyPlayersAnimationFinished(event.monotonic_time,
                                     event.target_property, event.group_id);

      return;
    }
  }
}

void ElementAnimations::NotifyAnimationTakeover(const AnimationEvent& event) {
  DCHECK(event.target_property == TargetProperty::SCROLL_OFFSET);
  if (!players_list_->empty()) {
    std::unique_ptr<AnimationCurve> animation_curve = event.curve->Clone();
    NotifyPlayersAnimationTakeover(event.monotonic_time, event.target_property,
                                   event.animation_start_time,
                                   std::move(animation_curve));
  }
}

void ElementAnimations::NotifyAnimationAborted(const AnimationEvent& event) {
  bool aborted_transform_animation = false;
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->group() == event.group_id &&
        animations_[i]->target_property() == event.target_property) {
      animations_[i]->SetRunState(Animation::ABORTED, event.monotonic_time);
      animations_[i]->set_received_finished_event(true);
      NotifyPlayersAnimationAborted(event.monotonic_time, event.target_property,
                                    event.group_id);
      if (event.target_property == TargetProperty::TRANSFORM)
        aborted_transform_animation = true;
      break;
    }
  }
  if (aborted_transform_animation)
    UpdatePotentiallyAnimatingTransform();
}

void ElementAnimations::NotifyAnimationPropertyUpdate(
    const AnimationEvent& event) {
  bool notify_active_observers = true;
  bool notify_pending_observers = true;
  switch (event.target_property) {
    case TargetProperty::OPACITY:
      NotifyObserversOpacityAnimated(event.opacity, notify_active_observers,
                                     notify_pending_observers);
      break;
    case TargetProperty::TRANSFORM:
      NotifyObserversTransformAnimated(event.transform, notify_active_observers,
                                       notify_pending_observers);
      break;
    default:
      NOTREACHED();
  }
}

bool ElementAnimations::HasFilterAnimationThatInflatesBounds() const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (!animations_[i]->is_finished() &&
        animations_[i]->target_property() == TargetProperty::FILTER &&
        animations_[i]
            ->curve()
            ->ToFilterAnimationCurve()
            ->HasFilterThatMovesPixels())
      return true;
  }

  return false;
}

bool ElementAnimations::HasTransformAnimationThatInflatesBounds() const {
  return IsCurrentlyAnimatingProperty(TargetProperty::TRANSFORM,
                                      LayerTreeType::ACTIVE) ||
         IsCurrentlyAnimatingProperty(TargetProperty::TRANSFORM,
                                      LayerTreeType::PENDING);
}

bool ElementAnimations::FilterAnimationBoundsForBox(const gfx::BoxF& box,
                                                    gfx::BoxF* bounds) const {
  // TODO(avallee): Implement.
  return false;
}

bool ElementAnimations::TransformAnimationBoundsForBox(
    const gfx::BoxF& box,
    gfx::BoxF* bounds) const {
  DCHECK(HasTransformAnimationThatInflatesBounds())
      << "TransformAnimationBoundsForBox will give incorrect results if there "
      << "are no transform animations affecting bounds, non-animated transform "
      << "is not known";

  // Compute bounds based on animations for which is_finished() is false.
  // Do nothing if there are no such animations; in this case, it is assumed
  // that callers will take care of computing bounds based on the owning layer's
  // actual transform.
  *bounds = gfx::BoxF();
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->is_finished() ||
        animations_[i]->target_property() != TargetProperty::TRANSFORM)
      continue;

    const TransformAnimationCurve* transform_animation_curve =
        animations_[i]->curve()->ToTransformAnimationCurve();
    gfx::BoxF animation_bounds;
    bool success =
        transform_animation_curve->AnimatedBoundsForBox(box, &animation_bounds);
    if (!success)
      return false;
    bounds->Union(animation_bounds);
  }

  return true;
}

bool ElementAnimations::HasAnimationThatAffectsScale() const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->is_finished() ||
        animations_[i]->target_property() != TargetProperty::TRANSFORM)
      continue;

    const TransformAnimationCurve* transform_animation_curve =
        animations_[i]->curve()->ToTransformAnimationCurve();
    if (transform_animation_curve->AffectsScale())
      return true;
  }

  return false;
}

bool ElementAnimations::HasOnlyTranslationTransforms(
    LayerTreeType observer_type) const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->is_finished() ||
        animations_[i]->target_property() != TargetProperty::TRANSFORM)
      continue;

    if ((observer_type == LayerTreeType::ACTIVE &&
         !animations_[i]->affects_active_observers()) ||
        (observer_type == LayerTreeType::PENDING &&
         !animations_[i]->affects_pending_observers()))
      continue;

    const TransformAnimationCurve* transform_animation_curve =
        animations_[i]->curve()->ToTransformAnimationCurve();
    if (!transform_animation_curve->IsTranslation())
      return false;
  }

  return true;
}

bool ElementAnimations::AnimationsPreserveAxisAlignment() const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->is_finished() ||
        animations_[i]->target_property() != TargetProperty::TRANSFORM)
      continue;

    const TransformAnimationCurve* transform_animation_curve =
        animations_[i]->curve()->ToTransformAnimationCurve();
    if (!transform_animation_curve->PreservesAxisAlignment())
      return false;
  }

  return true;
}

bool ElementAnimations::AnimationStartScale(LayerTreeType observer_type,
                                            float* start_scale) const {
  *start_scale = 0.f;
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->is_finished() ||
        animations_[i]->target_property() != TargetProperty::TRANSFORM)
      continue;

    if ((observer_type == LayerTreeType::ACTIVE &&
         !animations_[i]->affects_active_observers()) ||
        (observer_type == LayerTreeType::PENDING &&
         !animations_[i]->affects_pending_observers()))
      continue;

    bool forward_direction = true;
    switch (animations_[i]->direction()) {
      case Animation::DIRECTION_NORMAL:
      case Animation::DIRECTION_ALTERNATE:
        forward_direction = animations_[i]->playback_rate() >= 0.0;
        break;
      case Animation::DIRECTION_REVERSE:
      case Animation::DIRECTION_ALTERNATE_REVERSE:
        forward_direction = animations_[i]->playback_rate() < 0.0;
        break;
    }

    const TransformAnimationCurve* transform_animation_curve =
        animations_[i]->curve()->ToTransformAnimationCurve();
    float animation_start_scale = 0.f;
    if (!transform_animation_curve->AnimationStartScale(forward_direction,
                                                        &animation_start_scale))
      return false;
    *start_scale = std::max(*start_scale, animation_start_scale);
  }
  return true;
}

bool ElementAnimations::MaximumTargetScale(LayerTreeType observer_type,
                                           float* max_scale) const {
  *max_scale = 0.f;
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->is_finished() ||
        animations_[i]->target_property() != TargetProperty::TRANSFORM)
      continue;

    if ((observer_type == LayerTreeType::ACTIVE &&
         !animations_[i]->affects_active_observers()) ||
        (observer_type == LayerTreeType::PENDING &&
         !animations_[i]->affects_pending_observers()))
      continue;

    bool forward_direction = true;
    switch (animations_[i]->direction()) {
      case Animation::DIRECTION_NORMAL:
      case Animation::DIRECTION_ALTERNATE:
        forward_direction = animations_[i]->playback_rate() >= 0.0;
        break;
      case Animation::DIRECTION_REVERSE:
      case Animation::DIRECTION_ALTERNATE_REVERSE:
        forward_direction = animations_[i]->playback_rate() < 0.0;
        break;
    }

    const TransformAnimationCurve* transform_animation_curve =
        animations_[i]->curve()->ToTransformAnimationCurve();
    float animation_scale = 0.f;
    if (!transform_animation_curve->MaximumTargetScale(forward_direction,
                                                       &animation_scale))
      return false;
    *max_scale = std::max(*max_scale, animation_scale);
  }

  return true;
}

void ElementAnimations::PushNewAnimationsToImplThread(
    ElementAnimations* element_animations_impl) const {
  // Any new animations owned by the main thread's ElementAnimations are cloned
  // and added to the impl thread's ElementAnimations.
  for (size_t i = 0; i < animations_.size(); ++i) {
    // If the animation is already running on the impl thread, there is no
    // need to copy it over.
    if (element_animations_impl->GetAnimationById(animations_[i]->id()))
      continue;

    if (animations_[i]->target_property() == TargetProperty::SCROLL_OFFSET &&
        !animations_[i]
             ->curve()
             ->ToScrollOffsetAnimationCurve()
             ->HasSetInitialValue()) {
      gfx::ScrollOffset current_scroll_offset;
      if (element_animations_impl->needs_active_value_observations()) {
        current_scroll_offset =
            element_animations_impl->ScrollOffsetForAnimation();
      } else {
        // The owning layer isn't yet in the active tree, so the main thread
        // scroll offset will be up-to-date.
        current_scroll_offset = ScrollOffsetForAnimation();
      }
      animations_[i]->curve()->ToScrollOffsetAnimationCurve()->SetInitialValue(
          current_scroll_offset);
    }

    // The new animation should be set to run as soon as possible.
    Animation::RunState initial_run_state =
        Animation::WAITING_FOR_TARGET_AVAILABILITY;
    std::unique_ptr<Animation> to_add(
        animations_[i]->CloneAndInitialize(initial_run_state));
    DCHECK(!to_add->needs_synchronized_start_time());
    to_add->set_affects_active_observers(false);
    element_animations_impl->AddAnimation(std::move(to_add));
  }
}

static bool IsCompleted(
    Animation* animation,
    const ElementAnimations* main_thread_element_animations) {
  if (animation->is_impl_only()) {
    return (animation->run_state() == Animation::WAITING_FOR_DELETION);
  } else {
    return !main_thread_element_animations->GetAnimationById(animation->id());
  }
}

void ElementAnimations::RemoveAnimationsCompletedOnMainThread(
    ElementAnimations* element_animations_impl) const {
  bool removed_transform_animation = false;
  // Animations removed on the main thread should no longer affect pending
  // observers, and should stop affecting active observers after the next call
  // to ActivateAnimations. If already WAITING_FOR_DELETION, they can be removed
  // immediately.
  auto& animations = element_animations_impl->animations_;
  for (const auto& animation : animations) {
    if (IsCompleted(animation.get(), this)) {
      animation->set_affects_pending_observers(false);
      if (animation->target_property() == TargetProperty::TRANSFORM)
        removed_transform_animation = true;
    }
  }
  auto affects_active_only_and_is_waiting_for_deletion =
      [](const std::unique_ptr<Animation>& animation) {
        return animation->run_state() == Animation::WAITING_FOR_DELETION &&
               !animation->affects_pending_observers();
      };
  animations.erase(
      std::remove_if(animations.begin(), animations.end(),
                     affects_active_only_and_is_waiting_for_deletion),
      animations.end());

  if (removed_transform_animation)
    element_animations_impl->UpdatePotentiallyAnimatingTransform();
}

void ElementAnimations::PushPropertiesToImplThread(
    ElementAnimations* element_animations_impl) {
  for (size_t i = 0; i < animations_.size(); ++i) {
    Animation* current_impl =
        element_animations_impl->GetAnimationById(animations_[i]->id());
    if (current_impl)
      animations_[i]->PushPropertiesTo(current_impl);
  }
  element_animations_impl->scroll_offset_animation_was_interrupted_ =
      scroll_offset_animation_was_interrupted_;
  scroll_offset_animation_was_interrupted_ = false;
}

void ElementAnimations::StartAnimations(base::TimeTicks monotonic_time) {
  DCHECK(needs_to_start_animations_);
  needs_to_start_animations_ = false;
  // First collect running properties affecting each type of observer.
  TargetProperties blocked_properties_for_active_observers;
  TargetProperties blocked_properties_for_pending_observers;
  std::vector<size_t> animations_waiting_for_target;

  animations_waiting_for_target.reserve(animations_.size());
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->run_state() == Animation::STARTING ||
        animations_[i]->run_state() == Animation::RUNNING) {
      if (animations_[i]->affects_active_observers()) {
        blocked_properties_for_active_observers[animations_[i]
                                                    ->target_property()] = true;
      }
      if (animations_[i]->affects_pending_observers()) {
        blocked_properties_for_pending_observers[animations_[i]
                                                     ->target_property()] =
            true;
      }
    } else if (animations_[i]->run_state() ==
               Animation::WAITING_FOR_TARGET_AVAILABILITY) {
      animations_waiting_for_target.push_back(i);
    }
  }

  for (size_t i = 0; i < animations_waiting_for_target.size(); ++i) {
    // Collect all properties for animations with the same group id (they
    // should all also be in the list of animations).
    size_t animation_index = animations_waiting_for_target[i];
    Animation* animation_waiting_for_target =
        animations_[animation_index].get();
    // Check for the run state again even though the animation was waiting
    // for target because it might have changed the run state while handling
    // previous animation in this loop (if they belong to same group).
    if (animation_waiting_for_target->run_state() ==
        Animation::WAITING_FOR_TARGET_AVAILABILITY) {
      TargetProperties enqueued_properties;
      bool affects_active_observers =
          animation_waiting_for_target->affects_active_observers();
      bool affects_pending_observers =
          animation_waiting_for_target->affects_pending_observers();
      enqueued_properties[animation_waiting_for_target->target_property()] =
          true;
      for (size_t j = animation_index + 1; j < animations_.size(); ++j) {
        if (animation_waiting_for_target->group() == animations_[j]->group()) {
          enqueued_properties[animations_[j]->target_property()] = true;
          affects_active_observers |=
              animations_[j]->affects_active_observers();
          affects_pending_observers |=
              animations_[j]->affects_pending_observers();
        }
      }

      // Check to see if intersection of the list of properties affected by
      // the group and the list of currently blocked properties is null, taking
      // into account the type(s) of observers affected by the group. In any
      // case, the group's target properties need to be added to the lists of
      // blocked properties.
      bool null_intersection = true;
      static_assert(TargetProperty::FIRST_TARGET_PROPERTY == 0,
                    "TargetProperty must be 0-based enum");
      for (int property = TargetProperty::FIRST_TARGET_PROPERTY;
           property <= TargetProperty::LAST_TARGET_PROPERTY; ++property) {
        if (enqueued_properties[property]) {
          if (affects_active_observers) {
            if (blocked_properties_for_active_observers[property])
              null_intersection = false;
            else
              blocked_properties_for_active_observers[property] = true;
          }
          if (affects_pending_observers) {
            if (blocked_properties_for_pending_observers[property])
              null_intersection = false;
            else
              blocked_properties_for_pending_observers[property] = true;
          }
        }
      }

      // If the intersection is null, then we are free to start the animations
      // in the group.
      if (null_intersection) {
        animation_waiting_for_target->SetRunState(Animation::STARTING,
                                                  monotonic_time);
        for (size_t j = animation_index + 1; j < animations_.size(); ++j) {
          if (animation_waiting_for_target->group() ==
              animations_[j]->group()) {
            animations_[j]->SetRunState(Animation::STARTING, monotonic_time);
          }
        }
      } else {
        needs_to_start_animations_ = true;
      }
    }
  }
}

void ElementAnimations::PromoteStartedAnimations(base::TimeTicks monotonic_time,
                                                 AnimationEvents* events) {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->run_state() == Animation::STARTING &&
        animations_[i]->affects_active_observers()) {
      animations_[i]->SetRunState(Animation::RUNNING, monotonic_time);
      if (!animations_[i]->has_set_start_time() &&
          !animations_[i]->needs_synchronized_start_time())
        animations_[i]->set_start_time(monotonic_time);
      if (events) {
        base::TimeTicks start_time;
        if (animations_[i]->has_set_start_time())
          start_time = animations_[i]->start_time();
        else
          start_time = monotonic_time;
        AnimationEvent started_event(
            AnimationEvent::STARTED, layer_id_, animations_[i]->group(),
            animations_[i]->target_property(), start_time);
        started_event.is_impl_only = animations_[i]->is_impl_only();
        if (started_event.is_impl_only)
          NotifyAnimationStarted(started_event);
        else
          events->events_.push_back(started_event);
      }
    }
  }
}

void ElementAnimations::MarkFinishedAnimations(base::TimeTicks monotonic_time) {
  bool finished_transform_animation = false;
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (!animations_[i]->is_finished() &&
        animations_[i]->IsFinishedAt(monotonic_time)) {
      animations_[i]->SetRunState(Animation::FINISHED, monotonic_time);
      if (animations_[i]->target_property() == TargetProperty::TRANSFORM) {
        finished_transform_animation = true;
      }
    }
  }
  if (finished_transform_animation)
    UpdatePotentiallyAnimatingTransform();
}

void ElementAnimations::MarkAnimationsForDeletion(
    base::TimeTicks monotonic_time,
    AnimationEvents* events) {
  bool marked_animations_for_deletions = false;
  std::vector<size_t> animations_with_same_group_id;

  animations_with_same_group_id.reserve(animations_.size());
  // Non-aborted animations are marked for deletion after a corresponding
  // AnimationEvent::FINISHED event is sent or received. This means that if
  // we don't have an events vector, we must ensure that non-aborted animations
  // have received a finished event before marking them for deletion.
  for (size_t i = 0; i < animations_.size(); i++) {
    int group_id = animations_[i]->group();
    if (animations_[i]->run_state() == Animation::ABORTED) {
      if (events && !animations_[i]->is_impl_only()) {
        AnimationEvent aborted_event(
            AnimationEvent::ABORTED, layer_id_, group_id,
            animations_[i]->target_property(), monotonic_time);
        events->events_.push_back(aborted_event);
      }
      // If on the compositor or on the main thread and received finish event,
      // animation can be marked for deletion.
      if (events || animations_[i]->received_finished_event()) {
        animations_[i]->SetRunState(Animation::WAITING_FOR_DELETION,
                                    monotonic_time);
        marked_animations_for_deletions = true;
      }
      continue;
    }

    // If running on the compositor and need to complete an aborted animation
    // on the main thread.
    if (events &&
        animations_[i]->run_state() ==
            Animation::ABORTED_BUT_NEEDS_COMPLETION) {
      AnimationEvent aborted_event(AnimationEvent::TAKEOVER, layer_id_,
                                   group_id, animations_[i]->target_property(),
                                   monotonic_time);
      aborted_event.animation_start_time =
          (animations_[i]->start_time() - base::TimeTicks()).InSecondsF();
      const ScrollOffsetAnimationCurve* scroll_offset_animation_curve =
          animations_[i]->curve()->ToScrollOffsetAnimationCurve();
      aborted_event.curve = scroll_offset_animation_curve->Clone();
      // Notify the compositor that the animation is finished.
      NotifyPlayersAnimationFinished(aborted_event.monotonic_time,
                                     aborted_event.target_property,
                                     aborted_event.group_id);
      // Notify main thread.
      events->events_.push_back(aborted_event);

      // Remove the animation from the compositor.
      animations_[i]->SetRunState(Animation::WAITING_FOR_DELETION,
                                  monotonic_time);
      marked_animations_for_deletions = true;
      continue;
    }

    bool all_anims_with_same_id_are_finished = false;

    // Since deleting an animation on the main thread leads to its deletion
    // on the impl thread, we only mark a FINISHED main thread animation for
    // deletion once it has received a FINISHED event from the impl thread.
    bool animation_i_will_send_or_has_received_finish_event =
        animations_[i]->is_controlling_instance() ||
        animations_[i]->is_impl_only() ||
        animations_[i]->received_finished_event();
    // If an animation is finished, and not already marked for deletion,
    // find out if all other animations in the same group are also finished.
    if (animations_[i]->run_state() == Animation::FINISHED &&
        animation_i_will_send_or_has_received_finish_event) {
      // Clear the animations_with_same_group_id if it was added for
      // the previous animation's iteration.
      if (animations_with_same_group_id.size() > 0)
        animations_with_same_group_id.clear();
      all_anims_with_same_id_are_finished = true;
      for (size_t j = 0; j < animations_.size(); ++j) {
        bool animation_j_will_send_or_has_received_finish_event =
            animations_[j]->is_controlling_instance() ||
            animations_[j]->is_impl_only() ||
            animations_[j]->received_finished_event();
        if (group_id == animations_[j]->group()) {
          if (!animations_[j]->is_finished() ||
              (animations_[j]->run_state() == Animation::FINISHED &&
               !animation_j_will_send_or_has_received_finish_event)) {
            all_anims_with_same_id_are_finished = false;
            break;
          } else if (j >= i &&
                     animations_[j]->run_state() != Animation::ABORTED) {
            // Mark down the animations which belong to the same group
            // and is not yet aborted. If this current iteration finds that all
            // animations with same ID are finished, then the marked
            // animations below will be set to WAITING_FOR_DELETION in next
            // iteration.
            animations_with_same_group_id.push_back(j);
          }
        }
      }
    }
    if (all_anims_with_same_id_are_finished) {
      // We now need to remove all animations with the same group id as
      // group_id (and send along animation finished notifications, if
      // necessary).
      for (size_t j = 0; j < animations_with_same_group_id.size(); j++) {
        size_t animation_index = animations_with_same_group_id[j];
        if (events) {
          AnimationEvent finished_event(
              AnimationEvent::FINISHED, layer_id_,
              animations_[animation_index]->group(),
              animations_[animation_index]->target_property(), monotonic_time);
          finished_event.is_impl_only =
              animations_[animation_index]->is_impl_only();
          if (finished_event.is_impl_only)
            NotifyAnimationFinished(finished_event);
          else
            events->events_.push_back(finished_event);
        }
        animations_[animation_index]->SetRunState(
            Animation::WAITING_FOR_DELETION, monotonic_time);
      }
      marked_animations_for_deletions = true;
    }
  }
  if (marked_animations_for_deletions)
    NotifyObserversAnimationWaitingForDeletion();
}

void ElementAnimations::MarkAbortedAnimationsForDeletion(
    ElementAnimations* element_animations_impl) const {
  bool aborted_transform_animation = false;
  auto& animations_impl = element_animations_impl->animations_;
  for (const auto& animation_impl : animations_impl) {
    // If the animation has been aborted on the main thread, mark it for
    // deletion.
    if (Animation* animation = GetAnimationById(animation_impl->id())) {
      if (animation->run_state() == Animation::ABORTED) {
        animation_impl->SetRunState(Animation::WAITING_FOR_DELETION,
                                    element_animations_impl->last_tick_time_);
        animation->SetRunState(Animation::WAITING_FOR_DELETION,
                               last_tick_time_);
        if (animation_impl->target_property() == TargetProperty::TRANSFORM) {
          aborted_transform_animation = true;
        }
      }
    }
  }

  if (aborted_transform_animation)
    element_animations_impl->UpdatePotentiallyAnimatingTransform();
}

void ElementAnimations::PurgeAnimationsMarkedForDeletion() {
  animations_.erase(
      std::remove_if(animations_.begin(), animations_.end(),
                     [](const std::unique_ptr<Animation>& animation) {
                       return animation->run_state() ==
                              Animation::WAITING_FOR_DELETION;
                     }),
      animations_.end());
}

void ElementAnimations::TickAnimations(base::TimeTicks monotonic_time) {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->run_state() == Animation::STARTING ||
        animations_[i]->run_state() == Animation::RUNNING ||
        animations_[i]->run_state() == Animation::PAUSED) {
      if (!animations_[i]->InEffect(monotonic_time))
        continue;

      base::TimeDelta trimmed =
          animations_[i]->TrimTimeToCurrentIteration(monotonic_time);

      switch (animations_[i]->target_property()) {
        case TargetProperty::TRANSFORM: {
          const TransformAnimationCurve* transform_animation_curve =
              animations_[i]->curve()->ToTransformAnimationCurve();
          const gfx::Transform transform =
              transform_animation_curve->GetValue(trimmed);
          NotifyObserversTransformAnimated(
              transform, animations_[i]->affects_active_observers(),
              animations_[i]->affects_pending_observers());
          break;
        }

        case TargetProperty::OPACITY: {
          const FloatAnimationCurve* float_animation_curve =
              animations_[i]->curve()->ToFloatAnimationCurve();
          const float opacity = std::max(
              std::min(float_animation_curve->GetValue(trimmed), 1.0f), 0.f);
          NotifyObserversOpacityAnimated(
              opacity, animations_[i]->affects_active_observers(),
              animations_[i]->affects_pending_observers());
          break;
        }

        case TargetProperty::FILTER: {
          const FilterAnimationCurve* filter_animation_curve =
              animations_[i]->curve()->ToFilterAnimationCurve();
          const FilterOperations filter =
              filter_animation_curve->GetValue(trimmed);
          NotifyObserversFilterAnimated(
              filter, animations_[i]->affects_active_observers(),
              animations_[i]->affects_pending_observers());
          break;
        }

        case TargetProperty::BACKGROUND_COLOR: {
          // Not yet implemented.
          break;
        }

        case TargetProperty::SCROLL_OFFSET: {
          const ScrollOffsetAnimationCurve* scroll_offset_animation_curve =
              animations_[i]->curve()->ToScrollOffsetAnimationCurve();
          const gfx::ScrollOffset scroll_offset =
              scroll_offset_animation_curve->GetValue(trimmed);
          NotifyObserversScrollOffsetAnimated(
              scroll_offset, animations_[i]->affects_active_observers(),
              animations_[i]->affects_pending_observers());
          break;
        }
      }
    }
  }
}

void ElementAnimations::UpdateActivation(UpdateActivationType type) {
  bool force = type == FORCE_ACTIVATION;
  if (animation_host_) {
    bool was_active = is_active_;
    is_active_ = false;
    for (size_t i = 0; i < animations_.size(); ++i) {
      if (animations_[i]->run_state() != Animation::WAITING_FOR_DELETION) {
        is_active_ = true;
        break;
      }
    }

    if (is_active_ && (!was_active || force))
      animation_host_->DidActivateElementAnimations(this);
    else if (!is_active_ && (was_active || force))
      animation_host_->DidDeactivateElementAnimations(this);
  }
}

void ElementAnimations::NotifyObserversOpacityAnimated(
    float opacity,
    bool notify_active_observers,
    bool notify_pending_observers) {
  if (notify_active_observers && needs_active_value_observations())
    OnOpacityAnimated(LayerTreeType::ACTIVE, opacity);
  if (notify_pending_observers && needs_pending_value_observations())
    OnOpacityAnimated(LayerTreeType::PENDING, opacity);
}

void ElementAnimations::NotifyObserversTransformAnimated(
    const gfx::Transform& transform,
    bool notify_active_observers,
    bool notify_pending_observers) {
  if (notify_active_observers && needs_active_value_observations())
    OnTransformAnimated(LayerTreeType::ACTIVE, transform);
  if (notify_pending_observers && needs_pending_value_observations())
    OnTransformAnimated(LayerTreeType::PENDING, transform);
}

void ElementAnimations::NotifyObserversFilterAnimated(
    const FilterOperations& filters,
    bool notify_active_observers,
    bool notify_pending_observers) {
  if (notify_active_observers && needs_active_value_observations())
    OnFilterAnimated(LayerTreeType::ACTIVE, filters);
  if (notify_pending_observers && needs_pending_value_observations())
    OnFilterAnimated(LayerTreeType::PENDING, filters);
}

void ElementAnimations::NotifyObserversScrollOffsetAnimated(
    const gfx::ScrollOffset& scroll_offset,
    bool notify_active_observers,
    bool notify_pending_observers) {
  if (notify_active_observers && needs_active_value_observations())
    OnScrollOffsetAnimated(LayerTreeType::ACTIVE, scroll_offset);
  if (notify_pending_observers && needs_pending_value_observations())
    OnScrollOffsetAnimated(LayerTreeType::PENDING, scroll_offset);
}

void ElementAnimations::NotifyObserversAnimationWaitingForDeletion() {
  OnAnimationWaitingForDeletion();
}

void ElementAnimations::NotifyObserversTransformIsPotentiallyAnimatingChanged(
    bool notify_active_observers,
    bool notify_pending_observers) {
  if (notify_active_observers && needs_active_value_observations())
    OnTransformIsPotentiallyAnimatingChanged(
        LayerTreeType::ACTIVE,
        potentially_animating_transform_for_active_observers_);
  if (notify_pending_observers && needs_pending_value_observations())
    OnTransformIsPotentiallyAnimatingChanged(
        LayerTreeType::PENDING,
        potentially_animating_transform_for_pending_observers_);
}

void ElementAnimations::UpdatePotentiallyAnimatingTransform() {
  bool was_potentially_animating_transform_for_active_observers =
      potentially_animating_transform_for_active_observers_;
  bool was_potentially_animating_transform_for_pending_observers =
      potentially_animating_transform_for_pending_observers_;

  potentially_animating_transform_for_active_observers_ = false;
  potentially_animating_transform_for_pending_observers_ = false;

  for (const auto& animation : animations_) {
    if (!animation->is_finished() &&
        animation->target_property() == TargetProperty::TRANSFORM) {
      potentially_animating_transform_for_active_observers_ |=
          animation->affects_active_observers();
      potentially_animating_transform_for_pending_observers_ |=
          animation->affects_pending_observers();
    }
  }

  bool changed_for_active_observers =
      was_potentially_animating_transform_for_active_observers !=
      potentially_animating_transform_for_active_observers_;
  bool changed_for_pending_observers =
      was_potentially_animating_transform_for_pending_observers !=
      potentially_animating_transform_for_pending_observers_;

  if (!changed_for_active_observers && !changed_for_pending_observers)
    return;

  NotifyObserversTransformIsPotentiallyAnimatingChanged(
      changed_for_active_observers, changed_for_pending_observers);
}

bool ElementAnimations::HasActiveAnimation() const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (!animations_[i]->is_finished())
      return true;
  }
  return false;
}

bool ElementAnimations::IsPotentiallyAnimatingProperty(
    TargetProperty::Type target_property,
    LayerTreeType observer_type) const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (!animations_[i]->is_finished() &&
        animations_[i]->target_property() == target_property) {
      if ((observer_type == LayerTreeType::ACTIVE &&
           animations_[i]->affects_active_observers()) ||
          (observer_type == LayerTreeType::PENDING &&
           animations_[i]->affects_pending_observers()))
        return true;
    }
  }
  return false;
}

bool ElementAnimations::IsCurrentlyAnimatingProperty(
    TargetProperty::Type target_property,
    LayerTreeType observer_type) const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (!animations_[i]->is_finished() &&
        animations_[i]->InEffect(last_tick_time_) &&
        animations_[i]->target_property() == target_property) {
      if ((observer_type == LayerTreeType::ACTIVE &&
           animations_[i]->affects_active_observers()) ||
          (observer_type == LayerTreeType::PENDING &&
           animations_[i]->affects_pending_observers()))
        return true;
    }
  }
  return false;
}

void ElementAnimations::PauseAnimation(int animation_id,
                                       base::TimeDelta time_offset) {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->id() == animation_id) {
      animations_[i]->SetRunState(Animation::PAUSED,
                                  time_offset + animations_[i]->start_time() +
                                      animations_[i]->time_offset());
    }
  }
}

void ElementAnimations::RemoveAnimation(int animation_id) {
  bool removed_transform_animation = false;
  // Since we want to use the animations that we're going to remove, we need to
  // use a stable_parition here instead of remove_if. Remove_if leaves the
  // removed items in an unspecified state.
  auto animations_to_remove = std::stable_partition(
      animations_.begin(), animations_.end(),
      [animation_id](const std::unique_ptr<Animation>& animation) {
        return animation->id() != animation_id;
      });
  for (auto it = animations_to_remove; it != animations_.end(); ++it) {
    if ((*it)->target_property() == TargetProperty::SCROLL_OFFSET) {
      scroll_offset_animation_was_interrupted_ = true;
    } else if ((*it)->target_property() == TargetProperty::TRANSFORM &&
               !(*it)->is_finished()) {
      removed_transform_animation = true;
    }
  }

  animations_.erase(animations_to_remove, animations_.end());
  UpdateActivation(NORMAL_ACTIVATION);
  if (removed_transform_animation)
    UpdatePotentiallyAnimatingTransform();
}

void ElementAnimations::AbortAnimation(int animation_id) {
  bool aborted_transform_animation = false;
  if (Animation* animation = GetAnimationById(animation_id)) {
    if (!animation->is_finished()) {
      animation->SetRunState(Animation::ABORTED, last_tick_time_);
      if (animation->target_property() == TargetProperty::TRANSFORM)
        aborted_transform_animation = true;
    }
  }
  if (aborted_transform_animation)
    UpdatePotentiallyAnimatingTransform();
}

void ElementAnimations::AbortAnimations(TargetProperty::Type target_property,
                                        bool needs_completion) {
  if (needs_completion)
    DCHECK(target_property == TargetProperty::SCROLL_OFFSET);

  bool aborted_transform_animation = false;
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->target_property() == target_property &&
        !animations_[i]->is_finished()) {
      // Currently only impl-only scroll offset animations can be completed on
      // the main thread.
      if (needs_completion && animations_[i]->is_impl_only()) {
        animations_[i]->SetRunState(Animation::ABORTED_BUT_NEEDS_COMPLETION,
                                    last_tick_time_);
      } else {
        animations_[i]->SetRunState(Animation::ABORTED, last_tick_time_);
      }
      if (target_property == TargetProperty::TRANSFORM)
        aborted_transform_animation = true;
    }
  }
  if (aborted_transform_animation)
    UpdatePotentiallyAnimatingTransform();
}

Animation* ElementAnimations::GetAnimation(
    TargetProperty::Type target_property) const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    size_t index = animations_.size() - i - 1;
    if (animations_[index]->target_property() == target_property)
      return animations_[index].get();
  }
  return nullptr;
}

Animation* ElementAnimations::GetAnimationById(int animation_id) const {
  for (size_t i = 0; i < animations_.size(); ++i)
    if (animations_[i]->id() == animation_id)
      return animations_[i].get();
  return nullptr;
}

void ElementAnimations::OnFilterAnimated(LayerTreeType tree_type,
                                         const FilterOperations& filters) {
  DCHECK(layer_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetLayerFilterMutated(
      layer_id(), tree_type, filters);
}

void ElementAnimations::OnOpacityAnimated(LayerTreeType tree_type,
                                          float opacity) {
  DCHECK(layer_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetLayerOpacityMutated(
      layer_id(), tree_type, opacity);
}

void ElementAnimations::OnTransformAnimated(LayerTreeType tree_type,
                                            const gfx::Transform& transform) {
  DCHECK(layer_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetLayerTransformMutated(
      layer_id(), tree_type, transform);
}

void ElementAnimations::OnScrollOffsetAnimated(
    LayerTreeType tree_type,
    const gfx::ScrollOffset& scroll_offset) {
  DCHECK(layer_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetLayerScrollOffsetMutated(
      layer_id(), tree_type, scroll_offset);
}

void ElementAnimations::OnAnimationWaitingForDeletion() {
  // TODO(loyso): Invalidate AnimationHost::SetNeedsPushProperties here.
  // But we always do PushProperties in AnimationHost for now. crbug.com/604280
  DCHECK(animation_host());
  animation_host()->OnAnimationWaitingForDeletion();
}

void ElementAnimations::OnTransformIsPotentiallyAnimatingChanged(
    LayerTreeType tree_type,
    bool is_animating) {
  DCHECK(layer_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()
      ->mutator_host_client()
      ->LayerTransformIsPotentiallyAnimatingChanged(layer_id(), tree_type,
                                                    is_animating);
}

void ElementAnimations::NotifyPlayersAnimationStarted(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    int group) {
  for (PlayersListNode* node = players_list_->head();
       node != players_list_->end(); node = node->next()) {
    AnimationPlayer* player = node->value();
    player->NotifyAnimationStarted(monotonic_time, target_property, group);
  }
}

void ElementAnimations::NotifyPlayersAnimationFinished(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    int group) {
  for (PlayersListNode* node = players_list_->head();
       node != players_list_->end(); node = node->next()) {
    AnimationPlayer* player = node->value();
    player->NotifyAnimationFinished(monotonic_time, target_property, group);
  }
}

void ElementAnimations::NotifyPlayersAnimationAborted(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    int group) {
  for (PlayersListNode* node = players_list_->head();
       node != players_list_->end(); node = node->next()) {
    AnimationPlayer* player = node->value();
    player->NotifyAnimationAborted(monotonic_time, target_property, group);
  }
}

void ElementAnimations::NotifyPlayersAnimationTakeover(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    double animation_start_time,
    std::unique_ptr<AnimationCurve> curve) {
  DCHECK(curve);
  for (PlayersListNode* node = players_list_->head();
       node != players_list_->end(); node = node->next()) {
    std::unique_ptr<AnimationCurve> animation_curve = curve->Clone();
    AnimationPlayer* player = node->value();
    player->NotifyAnimationTakeover(monotonic_time, target_property,
                                    animation_start_time,
                                    std::move(animation_curve));
  }
}

gfx::ScrollOffset ElementAnimations::ScrollOffsetForAnimation() const {
  if (animation_host()) {
    DCHECK(animation_host()->mutator_host_client());
    return animation_host()->mutator_host_client()->GetScrollOffsetForAnimation(
        layer_id());
  }

  return gfx::ScrollOffset();
}

}  // namespace cc
