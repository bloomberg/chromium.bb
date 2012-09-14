// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCLayerAnimationController_h
#define CCLayerAnimationController_h

#include "CCAnimationEvents.h"

#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebKit {
class WebTransformationMatrix;
}

namespace cc {

class Animation;
class IntSize;
class KeyframeValueList;

class CCLayerAnimationControllerClient {
public:
    virtual ~CCLayerAnimationControllerClient() { }

    virtual int id() const = 0;
    virtual void setOpacityFromAnimation(float) = 0;
    virtual float opacity() const = 0;
    virtual void setTransformFromAnimation(const WebKit::WebTransformationMatrix&) = 0;
    virtual const WebKit::WebTransformationMatrix& transform() const = 0;
};

class CCLayerAnimationController {
    WTF_MAKE_NONCOPYABLE(CCLayerAnimationController);
public:
    static PassOwnPtr<CCLayerAnimationController> create(CCLayerAnimationControllerClient*);

    virtual ~CCLayerAnimationController();

    // These methods are virtual for testing.
    virtual void addAnimation(PassOwnPtr<CCActiveAnimation>);
    virtual void pauseAnimation(int animationId, double timeOffset);
    virtual void removeAnimation(int animationId);
    virtual void removeAnimation(int animationId, CCActiveAnimation::TargetProperty);
    virtual void suspendAnimations(double monotonicTime);
    virtual void resumeAnimations(double monotonicTime);

    // Ensures that the list of active animations on the main thread and the impl thread
    // are kept in sync. This function does not take ownership of the impl thread controller.
    virtual void pushAnimationUpdatesTo(CCLayerAnimationController*);

    void animate(double monotonicTime, CCAnimationEventsVector*);

    // Returns the active animation in the given group, animating the given property, if such an
    // animation exists.
    CCActiveAnimation* getActiveAnimation(int groupId, CCActiveAnimation::TargetProperty) const;

    // Returns the active animation animating the given property that is either running, or is
    // next to run, if such an animation exists.
    CCActiveAnimation* getActiveAnimation(CCActiveAnimation::TargetProperty) const;

    // Returns true if there are any animations that have neither finished nor aborted.
    bool hasActiveAnimation() const;

    // Returns true if there is an animation currently animating the given property, or
    // if there is an animation scheduled to animate this property in the future.
    bool isAnimatingProperty(CCActiveAnimation::TargetProperty) const;

    // This is called in response to an animation being started on the impl thread. This
    // function updates the corresponding main thread animation's start time.
    void notifyAnimationStarted(const CCAnimationEvent&);

    // If a sync is forced, then the next time animation updates are pushed to the impl
    // thread, all animations will be transferred.
    void setForceSync() { m_forceSync = true; }

    void setClient(CCLayerAnimationControllerClient*);

protected:
    explicit CCLayerAnimationController(CCLayerAnimationControllerClient*);

private:
    typedef HashSet<int, DefaultHash<int>::Hash, WTF::UnsignedWithZeroKeyHashTraits<int> > TargetProperties;

    void pushNewAnimationsToImplThread(CCLayerAnimationController*) const;
    void removeAnimationsCompletedOnMainThread(CCLayerAnimationController*) const;
    void pushPropertiesToImplThread(CCLayerAnimationController*) const;
    void replaceImplThreadAnimations(CCLayerAnimationController*) const;

    void startAnimationsWaitingForNextTick(double monotonicTime, CCAnimationEventsVector*);
    void startAnimationsWaitingForStartTime(double monotonicTime, CCAnimationEventsVector*);
    void startAnimationsWaitingForTargetAvailability(double monotonicTime, CCAnimationEventsVector*);
    void resolveConflicts(double monotonicTime);
    void markAnimationsForDeletion(double monotonicTime, CCAnimationEventsVector*);
    void purgeAnimationsMarkedForDeletion();

    void tickAnimations(double monotonicTime);

    // If this is true, we force a sync to the impl thread.
    bool m_forceSync;

    CCLayerAnimationControllerClient* m_client;
    Vector<OwnPtr<CCActiveAnimation> > m_activeAnimations;
};

} // namespace cc

#endif // CCLayerAnimationController_h
