// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYER_ANIMATION_CONTROLLER_H_
#define CC_LAYER_ANIMATION_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cc/animation_events.h"
#include "cc/cc_export.h"
#include "cc/scoped_ptr_vector.h"
#include "ui/gfx/transform.h"

namespace gfx {
class Transform;
}

namespace cc {

class Animation;
class AnimationRegistrar;
class KeyframeValueList;

class CC_EXPORT LayerAnimationController
    : public base::RefCounted<LayerAnimationController> {
public:
    static scoped_refptr<LayerAnimationController> create();

    // These methods are virtual for testing.
    virtual void addAnimation(scoped_ptr<ActiveAnimation>);
    virtual void pauseAnimation(int animationId, double timeOffset);
    virtual void removeAnimation(int animationId);
    virtual void removeAnimation(int animationId, ActiveAnimation::TargetProperty);
    virtual void suspendAnimations(double monotonicTime);
    virtual void resumeAnimations(double monotonicTime);

    // Ensures that the list of active animations on the main thread and the impl thread
    // are kept in sync. This function does not take ownership of the impl thread controller.
    virtual void pushAnimationUpdatesTo(LayerAnimationController*);

    void animate(double monotonicTime, AnimationEventsVector*);

    // Returns the active animation in the given group, animating the given property, if such an
    // animation exists.
    ActiveAnimation* getActiveAnimation(int groupId, ActiveAnimation::TargetProperty) const;

    // Returns the active animation animating the given property that is either running, or is
    // next to run, if such an animation exists.
    ActiveAnimation* getActiveAnimation(ActiveAnimation::TargetProperty) const;

    // Returns true if there are any animations that have neither finished nor aborted.
    bool hasActiveAnimation() const;

    // Returns true if there is an animation currently animating the given property, or
    // if there is an animation scheduled to animate this property in the future.
    bool isAnimatingProperty(ActiveAnimation::TargetProperty) const;

    // This is called in response to an animation being started on the impl thread. This
    // function updates the corresponding main thread animation's start time.
    void notifyAnimationStarted(const AnimationEvent&);

    // If a sync is forced, then the next time animation updates are pushed to the impl
    // thread, all animations will be transferred.
    void setForceSync() { m_forceSync = true; }

    void setAnimationRegistrar(AnimationRegistrar*);
    void setId(int id);

    // Gets the last animated opacity value.
    float opacity() const { return m_opacity; }

    // Sets the opacity. Returns true if this call actually updates the opacity.
    // This can return false if either the new opacity matches the old, or if
    // the property is currently animating.
    bool setOpacity(float opacity);

    // Gets the last animate transform value.
    const gfx::Transform& transform() const { return m_transform; }

    // Sets the transform. Returns true if this call actually updates the
    // transform. This can return false if either the new transform matches the
    // old or if the property is currently animating.
    bool setTransform(const gfx::Transform& transform);

protected:
    friend class base::RefCounted<LayerAnimationController>;

    LayerAnimationController();
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

    void updateRegistration();

    // If this is true, we force a sync to the impl thread.
    bool m_forceSync;

    float m_opacity;
    gfx::Transform m_transform;

    AnimationRegistrar* m_registrar;
    int m_id;
    ScopedPtrVector<ActiveAnimation> m_activeAnimations;

    DISALLOW_COPY_AND_ASSIGN(LayerAnimationController);
};

} // namespace cc

#endif  // CC_LAYER_ANIMATION_CONTROLLER_H_
