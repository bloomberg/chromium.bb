// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/layer_animation_controller.h"

#include <algorithm>

#include "cc/animation/animation.h"
#include "cc/animation/animation_registrar.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/layer_animation_value_observer.h"
#include "cc/base/scoped_ptr_algorithm.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAnimationDelegate.h"
#include "ui/gfx/transform.h"

namespace cc {

LayerAnimationController::LayerAnimationController(int id)
    : force_sync_(false),
      registrar_(0),
      id_(id),
      is_active_(false),
      last_tick_time_(0),
      layer_animation_delegate_(NULL) {}

LayerAnimationController::~LayerAnimationController() {
  if (registrar_)
    registrar_->UnregisterAnimationController(this);
}

scoped_refptr<LayerAnimationController> LayerAnimationController::Create(
    int id) {
  return make_scoped_refptr(new LayerAnimationController(id));
}

void LayerAnimationController::PauseAnimation(int animation_id,
                                              double time_offset) {
  for (size_t i = 0; i < active_animations_.size(); ++i) {
    if (active_animations_[i]->id() == animation_id) {
      active_animations_[i]->SetRunState(
          Animation::Paused, time_offset + active_animations_[i]->start_time());
    }
  }
}

struct HasAnimationId {
  explicit HasAnimationId(int id) : id_(id) {}
  bool operator()(Animation* animation) const {
    return animation->id() == id_;
  }

 private:
  int id_;
};

void LayerAnimationController::RemoveAnimation(int animation_id) {
  ScopedPtrVector<Animation>& animations = active_animations_;
  animations.erase(cc::remove_if(&animations,
                                 animations.begin(),
                                 animations.end(),
                                 HasAnimationId(animation_id)),
                   animations.end());
  UpdateActivation(NormalActivation);
}

struct HasAnimationIdAndProperty {
  HasAnimationIdAndProperty(int id, Animation::TargetProperty target_property)
      : id_(id), target_property_(target_property) {}
  bool operator()(Animation* animation) const {
    return animation->id() == id_ &&
        animation->target_property() == target_property_;
  }

 private:
  int id_;
  Animation::TargetProperty target_property_;
};

void LayerAnimationController::RemoveAnimation(
    int animation_id,
    Animation::TargetProperty target_property) {
  ScopedPtrVector<Animation>& animations = active_animations_;
  animations.erase(cc::remove_if(&animations,
                                 animations.begin(),
                                 animations.end(),
                                 HasAnimationIdAndProperty(animation_id,
                                                           target_property)),
                   animations.end());
  UpdateActivation(NormalActivation);
}

// According to render layer backing, these are for testing only.
void LayerAnimationController::SuspendAnimations(double monotonic_time) {
  for (size_t i = 0; i < active_animations_.size(); ++i) {
    if (!active_animations_[i]->is_finished())
      active_animations_[i]->SetRunState(Animation::Paused, monotonic_time);
  }
}

// Looking at GraphicsLayerCA, this appears to be the analog to
// SuspendAnimations, which is for testing.
void LayerAnimationController::ResumeAnimations(double monotonic_time) {
  for (size_t i = 0; i < active_animations_.size(); ++i) {
    if (active_animations_[i]->run_state() == Animation::Paused)
      active_animations_[i]->SetRunState(Animation::Running, monotonic_time);
  }
}

// Ensures that the list of active animations on the main thread and the impl
// thread are kept in sync.
void LayerAnimationController::PushAnimationUpdatesTo(
    LayerAnimationController* controller_impl) {
  DCHECK(this != controller_impl);
  if (force_sync_) {
    ReplaceImplThreadAnimations(controller_impl);
    force_sync_ = false;
  } else {
    PurgeAnimationsMarkedForDeletion();
    PushNewAnimationsToImplThread(controller_impl);

    // Remove finished impl side animations only after pushing,
    // and only after the animations are deleted on the main thread
    // this insures we will never push an animation twice.
    RemoveAnimationsCompletedOnMainThread(controller_impl);

    PushPropertiesToImplThread(controller_impl);
  }
  controller_impl->UpdateActivation(NormalActivation);
  UpdateActivation(NormalActivation);
}

void LayerAnimationController::TransferAnimationsTo(
    LayerAnimationController* other_controller) {
  other_controller->active_animations_.clear();
  active_animations_.swap(other_controller->active_animations_);
  UpdateActivation(NormalActivation);
  set_force_sync();
  other_controller->UpdateActivation(NormalActivation);
  other_controller->set_force_sync();
  other_controller->SetAnimationRegistrar(registrar_);
}

void LayerAnimationController::Animate(double monotonic_time) {
  if (!HasActiveValueObserver())
    return;

  StartAnimationsWaitingForNextTick(monotonic_time);
  StartAnimationsWaitingForStartTime(monotonic_time);
  StartAnimationsWaitingForTargetAvailability(monotonic_time);
  ResolveConflicts(monotonic_time);
  TickAnimations(monotonic_time);
  last_tick_time_ = monotonic_time;
}

void LayerAnimationController::AccumulatePropertyUpdates(
    double monotonic_time,
    AnimationEventsVector* events) {
  if (!events)
    return;

  for (size_t i = 0; i < active_animations_.size(); ++i) {
    Animation* animation = active_animations_[i];
    if (!animation->is_impl_only())
      continue;

    if (animation->target_property() == Animation::Opacity) {
      AnimationEvent event(AnimationEvent::PropertyUpdate,
                           id_,
                           animation->group(),
                           Animation::Opacity,
                           monotonic_time);
      event.opacity = animation->curve()->ToFloatAnimationCurve()->GetValue(
          monotonic_time);
      event.is_impl_only = true;
      events->push_back(event);
    } else if (animation->target_property() == Animation::Transform) {
      AnimationEvent event(AnimationEvent::PropertyUpdate,
                           id_,
                           animation->group(),
                           Animation::Transform,
                           monotonic_time);
      event.transform =
          animation->curve()->ToTransformAnimationCurve()->GetValue(
              monotonic_time);
      event.is_impl_only = true;
      events->push_back(event);
    }
  }
}

void LayerAnimationController::UpdateState(bool start_ready_animations,
                                           AnimationEventsVector* events) {
  if (!HasActiveValueObserver())
    return;

  if (start_ready_animations)
    PromoteStartedAnimations(last_tick_time_, events);

  MarkFinishedAnimations(last_tick_time_);
  MarkAnimationsForDeletion(last_tick_time_, events);

  if (start_ready_animations) {
    StartAnimationsWaitingForTargetAvailability(last_tick_time_);
    PromoteStartedAnimations(last_tick_time_, events);
  }

  AccumulatePropertyUpdates(last_tick_time_, events);

  UpdateActivation(NormalActivation);
}

void LayerAnimationController::AddAnimation(scoped_ptr<Animation> animation) {
  active_animations_.push_back(animation.Pass());
  UpdateActivation(NormalActivation);
}

Animation* LayerAnimationController::GetAnimation(
    int group_id,
    Animation::TargetProperty target_property) const {
  for (size_t i = 0; i < active_animations_.size(); ++i)
    if (active_animations_[i]->group() == group_id &&
        active_animations_[i]->target_property() == target_property)
      return active_animations_[i];
  return 0;
}

Animation* LayerAnimationController::GetAnimation(
    Animation::TargetProperty target_property) const {
  for (size_t i = 0; i < active_animations_.size(); ++i) {
    size_t index = active_animations_.size() - i - 1;
    if (active_animations_[index]->target_property() == target_property)
      return active_animations_[index];
  }
  return 0;
}

bool LayerAnimationController::HasActiveAnimation() const {
  for (size_t i = 0; i < active_animations_.size(); ++i) {
    if (!active_animations_[i]->is_finished())
      return true;
  }
  return false;
}

bool LayerAnimationController::IsAnimatingProperty(
    Animation::TargetProperty target_property) const {
  for (size_t i = 0; i < active_animations_.size(); ++i) {
    if (!active_animations_[i]->is_finished() &&
        active_animations_[i]->target_property() == target_property)
      return true;
  }
  return false;
}

void LayerAnimationController::SetAnimationRegistrar(
    AnimationRegistrar* registrar) {
  if (registrar_ == registrar)
    return;

  if (registrar_)
    registrar_->UnregisterAnimationController(this);

  registrar_ = registrar;
  if (registrar_)
    registrar_->RegisterAnimationController(this);

  UpdateActivation(ForceActivation);
}

void LayerAnimationController::NotifyAnimationStarted(
    const AnimationEvent& event,
    double wall_clock_time) {
  if (event.is_impl_only) {
    FOR_EACH_OBSERVER(LayerAnimationEventObserver, event_observers_,
                      OnAnimationStarted(event));
    if (layer_animation_delegate_)
      layer_animation_delegate_->notifyAnimationStarted(wall_clock_time);

    return;
  }

  for (size_t i = 0; i < active_animations_.size(); ++i) {
    if (active_animations_[i]->group() == event.group_id &&
        active_animations_[i]->target_property() == event.target_property &&
        active_animations_[i]->needs_synchronized_start_time()) {
      active_animations_[i]->set_needs_synchronized_start_time(false);
      active_animations_[i]->set_start_time(event.monotonic_time);

      FOR_EACH_OBSERVER(LayerAnimationEventObserver, event_observers_,
                        OnAnimationStarted(event));
      if (layer_animation_delegate_)
        layer_animation_delegate_->notifyAnimationStarted(wall_clock_time);

      return;
    }
  }
}

void LayerAnimationController::NotifyAnimationFinished(
    const AnimationEvent& event,
    double wall_clock_time) {
  if (event.is_impl_only) {
    if (layer_animation_delegate_)
      layer_animation_delegate_->notifyAnimationFinished(wall_clock_time);
    return;
  }

  for (size_t i = 0; i < active_animations_.size(); ++i) {
    if (active_animations_[i]->group() == event.group_id &&
        active_animations_[i]->target_property() == event.target_property) {
      active_animations_[i]->set_received_finished_event(true);
      if (layer_animation_delegate_)
        layer_animation_delegate_->notifyAnimationFinished(wall_clock_time);

      return;
    }
  }
}

void LayerAnimationController::NotifyAnimationPropertyUpdate(
    const AnimationEvent& event) {
  switch (event.target_property) {
    case Animation::Opacity:
      NotifyObserversOpacityAnimated(event.opacity);
      break;
    case Animation::Transform:
      NotifyObserversTransformAnimated(event.transform);
      break;
    default:
      NOTREACHED();
  }
}

void LayerAnimationController::AddValueObserver(
    LayerAnimationValueObserver* observer) {
  if (!value_observers_.HasObserver(observer))
    value_observers_.AddObserver(observer);
}

void LayerAnimationController::RemoveValueObserver(
    LayerAnimationValueObserver* observer) {
  value_observers_.RemoveObserver(observer);
}

void LayerAnimationController::AddEventObserver(
    LayerAnimationEventObserver* observer) {
  if (!event_observers_.HasObserver(observer))
    event_observers_.AddObserver(observer);
}

void LayerAnimationController::RemoveEventObserver(
    LayerAnimationEventObserver* observer) {
  event_observers_.RemoveObserver(observer);
}

void LayerAnimationController::PushNewAnimationsToImplThread(
    LayerAnimationController* controller_impl) const {
  // Any new animations owned by the main thread's controller are cloned and
  // add to the impl thread's controller.
  for (size_t i = 0; i < active_animations_.size(); ++i) {
    // If the animation is already running on the impl thread, there is no
    // need to copy it over.
    if (controller_impl->GetAnimation(active_animations_[i]->group(),
                                      active_animations_[i]->target_property()))
      continue;

    // If the animation is not running on the impl thread, it does not
    // necessarily mean that it needs to be copied over and started; it may
    // have already finished. In this case, the impl thread animation will
    // have already notified that it has started and the main thread animation
    // will no longer need
    // a synchronized start time.
    if (!active_animations_[i]->needs_synchronized_start_time())
      continue;

    // The new animation should be set to run as soon as possible.
    Animation::RunState initial_run_state =
        Animation::WaitingForTargetAvailability;
    double start_time = 0;
    scoped_ptr<Animation> to_add(active_animations_[i]->CloneAndInitialize(
        Animation::ControllingInstance, initial_run_state, start_time));
    DCHECK(!to_add->needs_synchronized_start_time());
    controller_impl->AddAnimation(to_add.Pass());
  }
}

struct IsCompleted {
  explicit IsCompleted(const LayerAnimationController& main_thread_controller)
      : main_thread_controller_(main_thread_controller) {}
  bool operator()(Animation* animation) const {
    if (animation->is_impl_only()) {
      return (animation->run_state() == Animation::WaitingForDeletion);
    } else {
      return !main_thread_controller_.GetAnimation(
          animation->group(),
          animation->target_property());
    }
  }

 private:
  const LayerAnimationController& main_thread_controller_;
};

void LayerAnimationController::RemoveAnimationsCompletedOnMainThread(
    LayerAnimationController* controller_impl) const {
  // Delete all impl thread animations for which there is no corresponding
  // main thread animation. Each iteration,
  // controller->active_animations_.size() is decremented or i is incremented
  // guaranteeing progress towards loop termination.
  ScopedPtrVector<Animation>& animations =
      controller_impl->active_animations_;
  animations.erase(cc::remove_if(&animations,
                                 animations.begin(),
                                 animations.end(),
                                 IsCompleted(*this)),
                   animations.end());
}

void LayerAnimationController::PushPropertiesToImplThread(
    LayerAnimationController* controller_impl) const {
  for (size_t i = 0; i < active_animations_.size(); ++i) {
    Animation* current_impl =
        controller_impl->GetAnimation(
            active_animations_[i]->group(),
            active_animations_[i]->target_property());
    if (current_impl)
      active_animations_[i]->PushPropertiesTo(current_impl);
  }
}

void LayerAnimationController::StartAnimationsWaitingForNextTick(
    double monotonic_time) {
  for (size_t i = 0; i < active_animations_.size(); ++i) {
    if (active_animations_[i]->run_state() == Animation::WaitingForNextTick)
      active_animations_[i]->SetRunState(Animation::Starting, monotonic_time);
  }
}

void LayerAnimationController::StartAnimationsWaitingForStartTime(
    double monotonic_time) {
  for (size_t i = 0; i < active_animations_.size(); ++i) {
    if (active_animations_[i]->run_state() == Animation::WaitingForStartTime &&
        active_animations_[i]->start_time() <= monotonic_time)
      active_animations_[i]->SetRunState(Animation::Starting, monotonic_time);
  }
}

void LayerAnimationController::StartAnimationsWaitingForTargetAvailability(
    double monotonic_time) {
  // First collect running properties.
  TargetProperties blocked_properties;
  for (size_t i = 0; i < active_animations_.size(); ++i) {
    if (active_animations_[i]->run_state() == Animation::Starting ||
        active_animations_[i]->run_state() == Animation::Running)
      blocked_properties.insert(active_animations_[i]->target_property());
  }

  for (size_t i = 0; i < active_animations_.size(); ++i) {
    if (active_animations_[i]->run_state() ==
        Animation::WaitingForTargetAvailability) {
      // Collect all properties for animations with the same group id (they
      // should all also be in the list of animations).
      TargetProperties enqueued_properties;
      enqueued_properties.insert(active_animations_[i]->target_property());
      for (size_t j = i + 1; j < active_animations_.size(); ++j) {
        if (active_animations_[i]->group() == active_animations_[j]->group())
          enqueued_properties.insert(active_animations_[j]->target_property());
      }

      // Check to see if intersection of the list of properties affected by
      // the group and the list of currently blocked properties is null. In
      // any case, the group's target properties need to be added to the list
      // of blocked properties.
      bool null_intersection = true;
      for (TargetProperties::iterator p_iter = enqueued_properties.begin();
           p_iter != enqueued_properties.end();
           ++p_iter) {
        if (!blocked_properties.insert(*p_iter).second)
          null_intersection = false;
      }

      // If the intersection is null, then we are free to start the animations
      // in the group.
      if (null_intersection) {
        active_animations_[i]->SetRunState(
            Animation::Starting, monotonic_time);
        for (size_t j = i + 1; j < active_animations_.size(); ++j) {
          if (active_animations_[i]->group() ==
              active_animations_[j]->group()) {
            active_animations_[j]->SetRunState(
                Animation::Starting, monotonic_time);
          }
        }
      }
    }
  }
}

void LayerAnimationController::PromoteStartedAnimations(
    double monotonic_time,
    AnimationEventsVector* events) {
  for (size_t i = 0; i < active_animations_.size(); ++i) {
    if (active_animations_[i]->run_state() == Animation::Starting) {
      active_animations_[i]->SetRunState(Animation::Running, monotonic_time);
      if (!active_animations_[i]->has_set_start_time())
        active_animations_[i]->set_start_time(monotonic_time);
      if (events) {
        AnimationEvent started_event(
            AnimationEvent::Started,
            id_,
            active_animations_[i]->group(),
            active_animations_[i]->target_property(),
            monotonic_time);
        started_event.is_impl_only = active_animations_[i]->is_impl_only();
        events->push_back(started_event);
      }
    }
  }
}

void LayerAnimationController::MarkFinishedAnimations(double monotonic_time) {
  for (size_t i = 0; i < active_animations_.size(); ++i) {
    if (active_animations_[i]->IsFinishedAt(monotonic_time) &&
        active_animations_[i]->run_state() != Animation::WaitingForDeletion)
      active_animations_[i]->SetRunState(Animation::Finished, monotonic_time);
  }
}

void LayerAnimationController::ResolveConflicts(double monotonic_time) {
  // Find any animations that are animating the same property and resolve the
  // confict. We could eventually blend, but for now we'll just abort the
  // previous animation (where 'previous' means: (1) has a prior start time or
  // (2) has an equal start time, but was added to the queue earlier, i.e.,
  // has a lower index in active_animations_).
  for (size_t i = 0; i < active_animations_.size(); ++i) {
    if (active_animations_[i]->run_state() == Animation::Starting ||
        active_animations_[i]->run_state() == Animation::Running) {
      for (size_t j = i + 1; j < active_animations_.size(); ++j) {
        if ((active_animations_[j]->run_state() == Animation::Starting ||
             active_animations_[j]->run_state() == Animation::Running) &&
            active_animations_[i]->target_property() ==
            active_animations_[j]->target_property()) {
          if (active_animations_[i]->start_time() >
              active_animations_[j]->start_time()) {
            active_animations_[j]->SetRunState(Animation::Aborted,
                                               monotonic_time);
          } else {
            active_animations_[i]->SetRunState(Animation::Aborted,
                                               monotonic_time);
          }
        }
      }
    }
  }
}

void LayerAnimationController::MarkAnimationsForDeletion(
    double monotonic_time, AnimationEventsVector* events) {
  // Non-aborted animations are marked for deletion after a corresponding
  // AnimationEvent::Finished event is sent or received. This means that if
  // we don't have an events vector, we must ensure that non-aborted animations
  // have received a finished event before marking them for deletion.
  for (size_t i = 0; i < active_animations_.size(); i++) {
    int group_id = active_animations_[i]->group();
    bool all_anims_with_same_id_are_finished = false;

    // Since deleting an animation on the main thread leads to its deletion
    // on the impl thread, we only mark a Finished main thread animation for
    // deletion once it has received a Finished event from the impl thread.
    bool animation_i_will_send_or_has_received_finish_event =
        events || active_animations_[i]->received_finished_event();
    // If an animation is finished, and not already marked for deletion,
    // find out if all other animations in the same group are also finished.
    if (active_animations_[i]->run_state() == Animation::Aborted ||
        (active_animations_[i]->run_state() == Animation::Finished &&
         animation_i_will_send_or_has_received_finish_event)) {
      all_anims_with_same_id_are_finished = true;
      for (size_t j = 0; j < active_animations_.size(); ++j) {
        bool animation_j_will_send_or_has_received_finish_event =
            events || active_animations_[j]->received_finished_event();
        if (group_id == active_animations_[j]->group() &&
            (!active_animations_[j]->is_finished() ||
             (active_animations_[j]->run_state() == Animation::Finished &&
              !animation_j_will_send_or_has_received_finish_event))) {
          all_anims_with_same_id_are_finished = false;
          break;
        }
      }
    }
    if (all_anims_with_same_id_are_finished) {
      // We now need to remove all animations with the same group id as
      // group_id (and send along animation finished notifications, if
      // necessary).
      for (size_t j = i; j < active_animations_.size(); j++) {
        if (group_id == active_animations_[j]->group()) {
          if (events) {
            AnimationEvent finished_event(
                AnimationEvent::Finished,
                id_,
                active_animations_[j]->group(),
                active_animations_[j]->target_property(),
                monotonic_time);
            finished_event.is_impl_only = active_animations_[j]->is_impl_only();
            events->push_back(finished_event);
          }
          active_animations_[j]->SetRunState(Animation::WaitingForDeletion,
                                             monotonic_time);
        }
      }
    }
  }
}

static bool IsWaitingForDeletion(Animation* animation) {
  return animation->run_state() == Animation::WaitingForDeletion;
}

void LayerAnimationController::PurgeAnimationsMarkedForDeletion() {
  ScopedPtrVector<Animation>& animations = active_animations_;
  animations.erase(cc::remove_if(&animations,
                                 animations.begin(),
                                 animations.end(),
                                 IsWaitingForDeletion),
                   animations.end());
}

void LayerAnimationController::ReplaceImplThreadAnimations(
    LayerAnimationController* controller_impl) const {
  controller_impl->active_animations_.clear();
  for (size_t i = 0; i < active_animations_.size(); ++i) {
    scoped_ptr<Animation> to_add;
    if (active_animations_[i]->needs_synchronized_start_time()) {
      // We haven't received an animation started notification yet, so it
      // is important that we add it in a 'waiting' and not 'running' state.
      Animation::RunState initial_run_state =
          Animation::WaitingForTargetAvailability;
      double start_time = 0;
      to_add = active_animations_[i]->CloneAndInitialize(
          Animation::ControllingInstance,
          initial_run_state, start_time).Pass();
    } else {
      to_add = active_animations_[i]->Clone(
          Animation::ControllingInstance).Pass();
    }

    controller_impl->AddAnimation(to_add.Pass());
  }
}

void LayerAnimationController::TickAnimations(double monotonic_time) {
  for (size_t i = 0; i < active_animations_.size(); ++i) {
    if (active_animations_[i]->run_state() == Animation::Starting ||
        active_animations_[i]->run_state() == Animation::Running ||
        active_animations_[i]->run_state() == Animation::Paused) {
      double trimmed =
          active_animations_[i]->TrimTimeToCurrentIteration(monotonic_time);

      // Animation assumes its initial value until it gets the synchronized
      // start time from the impl thread and can start ticking.
      if (active_animations_[i]->needs_synchronized_start_time())
        trimmed = 0;

      // A just-started animation assumes its initial value.
      if (active_animations_[i]->run_state() == Animation::Starting &&
          !active_animations_[i]->has_set_start_time())
        trimmed = 0;

      switch (active_animations_[i]->target_property()) {
        case Animation::Transform: {
          const TransformAnimationCurve* transform_animation_curve =
              active_animations_[i]->curve()->ToTransformAnimationCurve();
          const gfx::Transform transform =
              transform_animation_curve->GetValue(trimmed);
          NotifyObserversTransformAnimated(transform);
          break;
        }

        case Animation::Opacity: {
          const FloatAnimationCurve* float_animation_curve =
              active_animations_[i]->curve()->ToFloatAnimationCurve();
          const float opacity = float_animation_curve->GetValue(trimmed);
          NotifyObserversOpacityAnimated(opacity);
          break;
        }

          // Do nothing for sentinel value.
        case Animation::TargetPropertyEnumSize:
          NOTREACHED();
      }
    }
  }
}

void LayerAnimationController::UpdateActivation(UpdateActivationType type) {
  bool force = type == ForceActivation;
  if (registrar_) {
    if (!active_animations_.empty() && (!is_active_ || force))
      registrar_->DidActivateAnimationController(this);
    else if (active_animations_.empty() && (is_active_ || force))
      registrar_->DidDeactivateAnimationController(this);
    is_active_ = !active_animations_.empty();
  }
}

void LayerAnimationController::NotifyObserversOpacityAnimated(float opacity) {
  FOR_EACH_OBSERVER(LayerAnimationValueObserver,
                    value_observers_,
                    OnOpacityAnimated(opacity));
}

void LayerAnimationController::NotifyObserversTransformAnimated(
    const gfx::Transform& transform) {
  FOR_EACH_OBSERVER(LayerAnimationValueObserver,
                    value_observers_,
                    OnTransformAnimated(transform));
}

bool LayerAnimationController::HasActiveValueObserver() {
  if (value_observers_.might_have_observers()) {
    ObserverListBase<LayerAnimationValueObserver>::Iterator it(
        value_observers_);
    LayerAnimationValueObserver* obs;
    while ((obs = it.GetNext()) != NULL)
      if (obs->IsActive())
        return true;
  }
  return false;
}

}  // namespace cc
