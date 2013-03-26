// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_LAYER_ANIMATION_CONTROLLER_H_
#define CC_ANIMATION_LAYER_ANIMATION_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/layer_animation_event_observer.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"
#include "ui/gfx/transform.h"

namespace gfx { class Transform; }

namespace cc {

class Animation;
class AnimationRegistrar;
class KeyframeValueList;
class LayerAnimationValueObserver;

class CC_EXPORT LayerAnimationController
    : public base::RefCounted<LayerAnimationController>,
      public LayerAnimationEventObserver {
 public:
  static scoped_refptr<LayerAnimationController> Create(int id);

  int id() const { return id_; }

  // These methods are virtual for testing.
  virtual void AddAnimation(scoped_ptr<Animation> animation);
  virtual void PauseAnimation(int animation_id, double time_offset);
  virtual void RemoveAnimation(int animation_id);
  virtual void RemoveAnimation(int animation_id,
                               Animation::TargetProperty target_property);
  virtual void SuspendAnimations(double monotonic_time);
  virtual void ResumeAnimations(double monotonic_time);

  // Ensures that the list of active animations on the main thread and the impl
  // thread are kept in sync. This function does not take ownership of the impl
  // thread controller.
  virtual void PushAnimationUpdatesTo(
      LayerAnimationController* controller_impl);

  void Animate(double monotonic_time);
  void AccumulatePropertyUpdates(double monotonic_time,
                                 AnimationEventsVector* events);
  void UpdateState(AnimationEventsVector* events);

  // Returns the active animation in the given group, animating the given
  // property, if such an animation exists.
  Animation* GetAnimation(int group_id,
                          Animation::TargetProperty target_property) const;

  // Returns the active animation animating the given property that is either
  // running, or is next to run, if such an animation exists.
  Animation* GetAnimation(Animation::TargetProperty target_property) const;

  // Returns true if there are any animations that have neither finished nor
  // aborted.
  bool HasActiveAnimation() const;

  // Returns true if there are any animations at all to process.
  bool has_any_animation() const { return !active_animations_.empty(); }

  // Returns true if there is an animation currently animating the given
  // property, or if there is an animation scheduled to animate this property in
  // the future.
  bool IsAnimatingProperty(Animation::TargetProperty target_property) const;

  // This is called in response to an animation being started on the impl
  // thread. This function updates the corresponding main thread animation's
  // start time.
  virtual void OnAnimationStarted(const AnimationEvent& event) OVERRIDE;

  // If a sync is forced, then the next time animation updates are pushed to the
  // impl thread, all animations will be transferred.
  void set_force_sync() { force_sync_ = true; }

  void SetAnimationRegistrar(AnimationRegistrar* registrar);
  AnimationRegistrar* animation_registrar() { return registrar_; }

  void AddObserver(LayerAnimationValueObserver* observer);
  void RemoveObserver(LayerAnimationValueObserver* observer);

 protected:
  friend class base::RefCounted<LayerAnimationController>;

  explicit LayerAnimationController(int id);
  virtual ~LayerAnimationController();

 private:
  typedef base::hash_set<int> TargetProperties;

  void PushNewAnimationsToImplThread(
      LayerAnimationController* controller_impl) const;
  void RemoveAnimationsCompletedOnMainThread(
      LayerAnimationController* controller_impl) const;
  void PushPropertiesToImplThread(
      LayerAnimationController* controller_impl) const;
  void ReplaceImplThreadAnimations(
      LayerAnimationController* controller_impl) const;

  void StartAnimationsWaitingForNextTick(double monotonic_time);
  void StartAnimationsWaitingForStartTime(double monotonic_time);
  void StartAnimationsWaitingForTargetAvailability(double monotonic_time);
  void ResolveConflicts(double monotonic_time);
  void PromoteStartedAnimations(double monotonic_time,
                                AnimationEventsVector* events);
  void MarkFinishedAnimations(double monotonic_time);
  void MarkAnimationsForDeletion(double monotonic_time,
                                 AnimationEventsVector* events);
  void PurgeAnimationsMarkedForDeletion();

  void TickAnimations(double monotonic_time);

  enum UpdateActivationType {
    NormalActivation,
    ForceActivation
  };
  void UpdateActivation(UpdateActivationType type);

  void NotifyObserversOpacityAnimated(float opacity);
  void NotifyObserversTransformAnimated(const gfx::Transform& transform);

  bool HasActiveObserver();

  // If this is true, we force a sync to the impl thread.
  bool force_sync_;

  AnimationRegistrar* registrar_;
  int id_;
  ScopedPtrVector<Animation> active_animations_;

  // This is used to ensure that we don't spam the registrar.
  bool is_active_;

  double last_tick_time_;

  ObserverList<LayerAnimationValueObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(LayerAnimationController);
};

}  // namespace cc

#endif  // CC_ANIMATION_LAYER_ANIMATION_CONTROLLER_H_
