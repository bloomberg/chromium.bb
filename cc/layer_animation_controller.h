// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYER_ANIMATION_CONTROLLER_H_
#define CC_LAYER_ANIMATION_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "cc/animation_events.h"
#include "cc/cc_export.h"
#include "cc/layer_animation_event_observer.h"
#include "cc/scoped_ptr_vector.h"
#include "ui/gfx/transform.h"

namespace gfx {
class Transform;
}

namespace cc {

class Animation;
class AnimationRegistrar;
class KeyframeValueList;
class LayerAnimationValueObserver;

class CC_EXPORT LayerAnimationController
    : public base::RefCounted<LayerAnimationController>,
      public LayerAnimationEventObserver {
public:
    static scoped_refptr<LayerAnimationController> create(int id);

    int id() const { return m_id; }

    // These methods are virtual for testing.
    virtual void addAnimation(scoped_ptr<Animation>);
    virtual void pauseAnimation(int animationId, double timeOffset);
    virtual void removeAnimation(int animationId);
    virtual void removeAnimation(int animationId, Animation::TargetProperty);
    virtual void suspendAnimations(double monotonicTime);
    virtual void resumeAnimations(double monotonicTime);

    // Ensures that the list of active animations on the main thread and the impl thread
    // are kept in sync. This function does not take ownership of the impl thread controller.
    virtual void pushAnimationUpdatesTo(LayerAnimationController*);

    void animate(double monotonicTime, AnimationEventsVector*);

    // Returns the active animation in the given group, animating the given property, if such an
    // animation exists.
    Animation* getAnimation(int groupId, Animation::TargetProperty) const;

    // Returns the active animation animating the given property that is either running, or is
    // next to run, if such an animation exists.
    Animation* getAnimation(Animation::TargetProperty) const;

    // Returns true if there are any animations that have neither finished nor aborted.
    bool hasActiveAnimation() const;

    // Returns true if there are any animations at all to process.
    bool hasAnyAnimation() const { return !m_activeAnimations.isEmpty(); }

    // Returns true if there is an animation currently animating the given property, or
    // if there is an animation scheduled to animate this property in the future.
    bool isAnimatingProperty(Animation::TargetProperty) const;

    // This is called in response to an animation being started on the impl thread. This
    // function updates the corresponding main thread animation's start time.
    virtual void OnAnimationStarted(const AnimationEvent&) OVERRIDE;

    // If a sync is forced, then the next time animation updates are pushed to the impl
    // thread, all animations will be transferred.
    void setForceSync() { m_forceSync = true; }

    void setAnimationRegistrar(AnimationRegistrar*);

    void addObserver(LayerAnimationValueObserver*);
    void removeObserver(LayerAnimationValueObserver*);

protected:
    friend class base::RefCounted<LayerAnimationController>;

    LayerAnimationController(int id);
    virtual ~LayerAnimationController();

private:
    typedef base::hash_set<int> TargetProperties;

    void pushNewAnimationsToImplThread(LayerAnimationController*) const;
    void removeAnimationsCompletedOnMainThread(LayerAnimationController*) const;
    void pushPropertiesToImplThread(LayerAnimationController*) const;
    void replaceImplThreadAnimations(LayerAnimationController*) const;

    void startAnimationsWaitingForNextTick(double monotonicTime, AnimationEventsVector*);
    void startAnimationsWaitingForStartTime(double monotonicTime, AnimationEventsVector*);
    void startAnimationsWaitingForTargetAvailability(double monotonicTime, AnimationEventsVector*);
    void resolveConflicts(double monotonicTime);
    void markAnimationsForDeletion(double monotonicTime, AnimationEventsVector*);
    void purgeAnimationsMarkedForDeletion();

    void tickAnimations(double monotonicTime);

    void updateActivation(bool force = false);

    void notifyObserversOpacityAnimated(float opacity);
    void notifyObserversTransformAnimated(const gfx::Transform& transform);

    bool hasActiveObserver();

    // If this is true, we force a sync to the impl thread.
    bool m_forceSync;

    AnimationRegistrar* m_registrar;
    int m_id;
    ScopedPtrVector<Animation> m_activeAnimations;

    // This is used to ensure that we don't spam the registrar.
    bool m_isActive;

    ObserverList<LayerAnimationValueObserver> m_observers;

    DISALLOW_COPY_AND_ASSIGN(LayerAnimationController);
};

} // namespace cc

#endif  // CC_LAYER_ANIMATION_CONTROLLER_H_
