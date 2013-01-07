// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_animation_controller.h"

#include "cc/active_animation.h"
#include "cc/animation_registrar.h"
#include "cc/keyframed_animation_curve.h"
#include "cc/layer_animation_value_observer.h"
#include "ui/gfx/transform.h"

namespace {
gfx::Transform convertWebTransformationMatrixToTransform(const WebKit::WebTransformationMatrix& matrix)
{
    gfx::Transform transform;
    transform.matrix().setDouble(0, 0, matrix.m11());
    transform.matrix().setDouble(0, 1, matrix.m21());
    transform.matrix().setDouble(0, 2, matrix.m31());
    transform.matrix().setDouble(0, 3, matrix.m41());
    transform.matrix().setDouble(1, 0, matrix.m12());
    transform.matrix().setDouble(1, 1, matrix.m22());
    transform.matrix().setDouble(1, 2, matrix.m32());
    transform.matrix().setDouble(1, 3, matrix.m42());
    transform.matrix().setDouble(2, 0, matrix.m13());
    transform.matrix().setDouble(2, 1, matrix.m23());
    transform.matrix().setDouble(2, 2, matrix.m33());
    transform.matrix().setDouble(2, 3, matrix.m43());
    transform.matrix().setDouble(3, 0, matrix.m14());
    transform.matrix().setDouble(3, 1, matrix.m24());
    transform.matrix().setDouble(3, 2, matrix.m34());
    transform.matrix().setDouble(3, 3, matrix.m44());
    return transform;
}
}  // namespace

namespace cc {

LayerAnimationController::LayerAnimationController(int id)
    : m_forceSync(false)
    , m_id(id)
    , m_registrar(0)
    , m_isActive(false)
{
}

LayerAnimationController::~LayerAnimationController()
{
    if (m_registrar)
        m_registrar->UnregisterAnimationController(this);
}

scoped_refptr<LayerAnimationController> LayerAnimationController::create(int id)
{
    return make_scoped_refptr(new LayerAnimationController(id));
}

void LayerAnimationController::pauseAnimation(int animationId, double timeOffset)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->id() == animationId)
            m_activeAnimations[i]->setRunState(ActiveAnimation::Paused, timeOffset + m_activeAnimations[i]->startTime());
    }
}

void LayerAnimationController::removeAnimation(int animationId)
{
    for (size_t i = 0; i < m_activeAnimations.size();) {
        if (m_activeAnimations[i]->id() == animationId)
            m_activeAnimations.remove(i);
        else
            i++;
    }
    updateActivation();
}

void LayerAnimationController::removeAnimation(int animationId, ActiveAnimation::TargetProperty targetProperty)
{
    for (size_t i = 0; i < m_activeAnimations.size();) {
        if (m_activeAnimations[i]->id() == animationId && m_activeAnimations[i]->targetProperty() == targetProperty)
            m_activeAnimations.remove(i);
        else
            i++;
    }
    updateActivation();
}

// According to render layer backing, these are for testing only.
void LayerAnimationController::suspendAnimations(double monotonicTime)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (!m_activeAnimations[i]->isFinished())
            m_activeAnimations[i]->setRunState(ActiveAnimation::Paused, monotonicTime);
    }
}

// Looking at GraphicsLayerCA, this appears to be the analog to suspendAnimations, which is for testing.
void LayerAnimationController::resumeAnimations(double monotonicTime)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == ActiveAnimation::Paused)
            m_activeAnimations[i]->setRunState(ActiveAnimation::Running, monotonicTime);
    }
}

// Ensures that the list of active animations on the main thread and the impl thread
// are kept in sync.
void LayerAnimationController::pushAnimationUpdatesTo(LayerAnimationController* controllerImpl)
{
    if (m_forceSync) {
        replaceImplThreadAnimations(controllerImpl);
        m_forceSync = false;
    } else {
        purgeAnimationsMarkedForDeletion();
        pushNewAnimationsToImplThread(controllerImpl);

        // Remove finished impl side animations only after pushing,
        // and only after the animations are deleted on the main thread
        // this insures we will never push an animation twice.
        removeAnimationsCompletedOnMainThread(controllerImpl);

        pushPropertiesToImplThread(controllerImpl);
    }
    controllerImpl->updateActivation();
    updateActivation();
}

void LayerAnimationController::animate(double monotonicTime, AnimationEventsVector* events)
{
    if (!hasActiveObserver())
        return;

    startAnimationsWaitingForNextTick(monotonicTime, events);
    startAnimationsWaitingForStartTime(monotonicTime, events);
    startAnimationsWaitingForTargetAvailability(monotonicTime, events);
    resolveConflicts(monotonicTime);
    tickAnimations(monotonicTime);
    markAnimationsForDeletion(monotonicTime, events);
    startAnimationsWaitingForTargetAvailability(monotonicTime, events);

    updateActivation();
}

void LayerAnimationController::addAnimation(scoped_ptr<ActiveAnimation> animation)
{
    m_activeAnimations.append(animation.Pass());
    updateActivation();
}

ActiveAnimation* LayerAnimationController::getActiveAnimation(int groupId, ActiveAnimation::TargetProperty targetProperty) const
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i)
        if (m_activeAnimations[i]->group() == groupId && m_activeAnimations[i]->targetProperty() == targetProperty)
            return m_activeAnimations[i];
    return 0;
}

ActiveAnimation* LayerAnimationController::getActiveAnimation(ActiveAnimation::TargetProperty targetProperty) const
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        size_t index = m_activeAnimations.size() - i - 1;
        if (m_activeAnimations[index]->targetProperty() == targetProperty)
            return m_activeAnimations[index];
    }
    return 0;
}

bool LayerAnimationController::hasActiveAnimation() const
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (!m_activeAnimations[i]->isFinished())
            return true;
    }
    return false;
}

bool LayerAnimationController::isAnimatingProperty(ActiveAnimation::TargetProperty targetProperty) const
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() != ActiveAnimation::Finished && m_activeAnimations[i]->runState() != ActiveAnimation::Aborted && m_activeAnimations[i]->targetProperty() == targetProperty)
            return true;
    }
    return false;
}

void LayerAnimationController::OnAnimationStarted(const AnimationEvent& event)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->group() == event.groupId && m_activeAnimations[i]->targetProperty() == event.targetProperty && m_activeAnimations[i]->needsSynchronizedStartTime()) {
            m_activeAnimations[i]->setNeedsSynchronizedStartTime(false);
            m_activeAnimations[i]->setStartTime(event.monotonicTime);
            return;
        }
    }
}

void LayerAnimationController::setAnimationRegistrar(AnimationRegistrar* registrar)
{
    if (m_registrar == registrar)
        return;

    if (m_registrar)
        m_registrar->UnregisterAnimationController(this);

    m_registrar = registrar;
    if (m_registrar)
        m_registrar->RegisterAnimationController(this);

    bool force = true;
    updateActivation(force);
}

void LayerAnimationController::addObserver(LayerAnimationValueObserver* observer)
{
    if (!m_observers.HasObserver(observer))
        m_observers.AddObserver(observer);
}

void LayerAnimationController::removeObserver(LayerAnimationValueObserver* observer)
{
    m_observers.RemoveObserver(observer);
}

void LayerAnimationController::pushNewAnimationsToImplThread(LayerAnimationController* controllerImpl) const
{
    // Any new animations owned by the main thread's controller are cloned and adde to the impl thread's controller.
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        // If the animation is already running on the impl thread, there is no need to copy it over.
        if (controllerImpl->getActiveAnimation(m_activeAnimations[i]->group(), m_activeAnimations[i]->targetProperty()))
            continue;

        // If the animation is not running on the impl thread, it does not necessarily mean that it needs
        // to be copied over and started; it may have already finished. In this case, the impl thread animation
        // will have already notified that it has started and the main thread animation will no longer need
        // a synchronized start time.
        if (!m_activeAnimations[i]->needsSynchronizedStartTime())
            continue;

        // The new animation should be set to run as soon as possible.
        ActiveAnimation::RunState initialRunState = ActiveAnimation::WaitingForTargetAvailability;
        double startTime = 0;
        scoped_ptr<ActiveAnimation> toAdd(m_activeAnimations[i]->cloneAndInitialize(ActiveAnimation::ControllingInstance, initialRunState, startTime));
        DCHECK(!toAdd->needsSynchronizedStartTime());
        controllerImpl->addAnimation(toAdd.Pass());
    }
}

void LayerAnimationController::removeAnimationsCompletedOnMainThread(LayerAnimationController* controllerImpl) const
{
    // Delete all impl thread animations for which there is no corresponding main thread animation.
    // Each iteration, controller->m_activeAnimations.size() is decremented or i is incremented
    // guaranteeing progress towards loop termination.
    for (size_t i = 0; i < controllerImpl->m_activeAnimations.size();) {
        ActiveAnimation* current = getActiveAnimation(controllerImpl->m_activeAnimations[i]->group(), controllerImpl->m_activeAnimations[i]->targetProperty());
        if (!current)
            controllerImpl->m_activeAnimations.remove(i);
        else
            i++;
    }
}

void LayerAnimationController::pushPropertiesToImplThread(LayerAnimationController* controllerImpl) const
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        ActiveAnimation* currentImpl = controllerImpl->getActiveAnimation(m_activeAnimations[i]->group(), m_activeAnimations[i]->targetProperty());
        if (currentImpl)
            m_activeAnimations[i]->pushPropertiesTo(currentImpl);
    }
}

void LayerAnimationController::startAnimationsWaitingForNextTick(double monotonicTime, AnimationEventsVector* events)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == ActiveAnimation::WaitingForNextTick) {
            m_activeAnimations[i]->setRunState(ActiveAnimation::Running, monotonicTime);
            if (!m_activeAnimations[i]->hasSetStartTime())
                m_activeAnimations[i]->setStartTime(monotonicTime);
            if (events)
                events->push_back(AnimationEvent(AnimationEvent::Started, m_id, m_activeAnimations[i]->group(), m_activeAnimations[i]->targetProperty(), monotonicTime));
        }
    }
}

void LayerAnimationController::startAnimationsWaitingForStartTime(double monotonicTime, AnimationEventsVector* events)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == ActiveAnimation::WaitingForStartTime && m_activeAnimations[i]->startTime() <= monotonicTime) {
            m_activeAnimations[i]->setRunState(ActiveAnimation::Running, monotonicTime);
            if (events)
                events->push_back(AnimationEvent(AnimationEvent::Started, m_id, m_activeAnimations[i]->group(), m_activeAnimations[i]->targetProperty(), monotonicTime));
        }
    }
}

void LayerAnimationController::startAnimationsWaitingForTargetAvailability(double monotonicTime, AnimationEventsVector* events)
{
    // First collect running properties.
    TargetProperties blockedProperties;
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == ActiveAnimation::Running || m_activeAnimations[i]->runState() == ActiveAnimation::Finished)
            blockedProperties.insert(m_activeAnimations[i]->targetProperty());
    }

    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == ActiveAnimation::WaitingForTargetAvailability) {
            // Collect all properties for animations with the same group id (they should all also be in the list of animations).
            TargetProperties enqueuedProperties;
            enqueuedProperties.insert(m_activeAnimations[i]->targetProperty());
            for (size_t j = i + 1; j < m_activeAnimations.size(); ++j) {
                if (m_activeAnimations[i]->group() == m_activeAnimations[j]->group())
                    enqueuedProperties.insert(m_activeAnimations[j]->targetProperty());
            }

            // Check to see if intersection of the list of properties affected by the group and the list of currently
            // blocked properties is null. In any case, the group's target properties need to be added to the list
            // of blocked properties.
            bool nullIntersection = true;
            for (TargetProperties::iterator pIter = enqueuedProperties.begin(); pIter != enqueuedProperties.end(); ++pIter) {
                if (!blockedProperties.insert(*pIter).second)
                    nullIntersection = false;
            }

            // If the intersection is null, then we are free to start the animations in the group.
            if (nullIntersection) {
                m_activeAnimations[i]->setRunState(ActiveAnimation::Running, monotonicTime);
                if (!m_activeAnimations[i]->hasSetStartTime())
                    m_activeAnimations[i]->setStartTime(monotonicTime);
                if (events)
                    events->push_back(AnimationEvent(AnimationEvent::Started, m_id, m_activeAnimations[i]->group(), m_activeAnimations[i]->targetProperty(), monotonicTime));
                for (size_t j = i + 1; j < m_activeAnimations.size(); ++j) {
                    if (m_activeAnimations[i]->group() == m_activeAnimations[j]->group()) {
                        m_activeAnimations[j]->setRunState(ActiveAnimation::Running, monotonicTime);
                        if (!m_activeAnimations[j]->hasSetStartTime())
                            m_activeAnimations[j]->setStartTime(monotonicTime);
                    }
                }
            }
        }
    }
}

void LayerAnimationController::resolveConflicts(double monotonicTime)
{
    // Find any animations that are animating the same property and resolve the
    // confict. We could eventually blend, but for now we'll just abort the
    // previous animation (where 'previous' means: (1) has a prior start time or
    // (2) has an equal start time, but was added to the queue earlier, i.e.,
    // has a lower index in m_activeAnimations).
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == ActiveAnimation::Running) {
            for (size_t j = i + 1; j < m_activeAnimations.size(); ++j) {
                if (m_activeAnimations[j]->runState() == ActiveAnimation::Running && m_activeAnimations[i]->targetProperty() == m_activeAnimations[j]->targetProperty()) {
                    if (m_activeAnimations[i]->startTime() > m_activeAnimations[j]->startTime())
                        m_activeAnimations[j]->setRunState(ActiveAnimation::Aborted, monotonicTime);
                    else
                        m_activeAnimations[i]->setRunState(ActiveAnimation::Aborted, monotonicTime);
                }
            }
        }
    }
}

void LayerAnimationController::markAnimationsForDeletion(double monotonicTime, AnimationEventsVector* events)
{
    for (size_t i = 0; i < m_activeAnimations.size(); i++) {
        int groupId = m_activeAnimations[i]->group();
        bool allAnimsWithSameIdAreFinished = false;
        // If an animation is finished, and not already marked for deletion,
        // Find out if all other animations in the same group are also finished.
        if (m_activeAnimations[i]->isFinished()) {
            allAnimsWithSameIdAreFinished = true;
            for (size_t j = 0; j < m_activeAnimations.size(); ++j) {
                if (groupId == m_activeAnimations[j]->group() && !m_activeAnimations[j]->isFinished()) {
                    allAnimsWithSameIdAreFinished = false;
                    break;
                }
            }
        }
        if (allAnimsWithSameIdAreFinished) {
            // We now need to remove all animations with the same group id as groupId
            // (and send along animation finished notifications, if necessary).
            for (size_t j = i; j < m_activeAnimations.size(); j++) {
                if (groupId == m_activeAnimations[j]->group()) {
                    if (events)
                        events->push_back(AnimationEvent(AnimationEvent::Finished, m_id, m_activeAnimations[j]->group(), m_activeAnimations[j]->targetProperty(), monotonicTime));
                    m_activeAnimations[j]->setRunState(ActiveAnimation::WaitingForDeletion, monotonicTime);
                }
            }
        }
    }
}

void LayerAnimationController::purgeAnimationsMarkedForDeletion()
{
    for (size_t i = 0; i < m_activeAnimations.size();) {
        if (m_activeAnimations[i]->runState() == ActiveAnimation::WaitingForDeletion)
            m_activeAnimations.remove(i);
        else
            i++;
    }
}

void LayerAnimationController::replaceImplThreadAnimations(LayerAnimationController* controllerImpl) const
{
    controllerImpl->m_activeAnimations.clear();
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        scoped_ptr<ActiveAnimation> toAdd;
        if (m_activeAnimations[i]->needsSynchronizedStartTime()) {
            // We haven't received an animation started notification yet, so it
            // is important that we add it in a 'waiting' and not 'running' state.
            ActiveAnimation::RunState initialRunState = ActiveAnimation::WaitingForTargetAvailability;
            double startTime = 0;
            toAdd = m_activeAnimations[i]->cloneAndInitialize(ActiveAnimation::ControllingInstance, initialRunState, startTime).Pass();
        } else
            toAdd = m_activeAnimations[i]->clone(ActiveAnimation::ControllingInstance).Pass();

        controllerImpl->addAnimation(toAdd.Pass());
    }
}

void LayerAnimationController::tickAnimations(double monotonicTime)
{
    for (size_t i = 0; i < m_activeAnimations.size(); ++i) {
        if (m_activeAnimations[i]->runState() == ActiveAnimation::Running || m_activeAnimations[i]->runState() == ActiveAnimation::Paused) {
            double trimmed = m_activeAnimations[i]->trimTimeToCurrentIteration(monotonicTime);

            // Animation assumes its initial value until it gets the synchronized start time
            // from the impl thread and can start ticking.
            if (m_activeAnimations[i]->needsSynchronizedStartTime())
                trimmed = 0;

            switch (m_activeAnimations[i]->targetProperty()) {

            case ActiveAnimation::Transform: {
                const TransformAnimationCurve* transformAnimationCurve = m_activeAnimations[i]->curve()->toTransformAnimationCurve();
                const gfx::Transform transform = convertWebTransformationMatrixToTransform(transformAnimationCurve->getValue(trimmed));
                if (m_activeAnimations[i]->isFinishedAt(monotonicTime))
                    m_activeAnimations[i]->setRunState(ActiveAnimation::Finished, monotonicTime);

                notifyObserversTransformAnimated(transform);
                break;
            }

            case ActiveAnimation::Opacity: {
                const FloatAnimationCurve* floatAnimationCurve = m_activeAnimations[i]->curve()->toFloatAnimationCurve();
                const float opacity = floatAnimationCurve->getValue(trimmed);
                if (m_activeAnimations[i]->isFinishedAt(monotonicTime))
                    m_activeAnimations[i]->setRunState(ActiveAnimation::Finished, monotonicTime);

                notifyObserversOpacityAnimated(opacity);
                break;
            }

            // Do nothing for sentinel value.
            case ActiveAnimation::TargetPropertyEnumSize:
                NOTREACHED();
            }
        }
    }
}

void LayerAnimationController::updateActivation(bool force)
{
    if (m_registrar) {
        if (!m_activeAnimations.isEmpty() && (!m_isActive || force))
            m_registrar->DidActivateAnimationController(this);
        else if (m_activeAnimations.isEmpty() && (m_isActive || force))
            m_registrar->DidDeactivateAnimationController(this);
        m_isActive = !m_activeAnimations.isEmpty();
    }
}

void LayerAnimationController::notifyObserversOpacityAnimated(float opacity)
{
    FOR_EACH_OBSERVER(LayerAnimationValueObserver,
                      m_observers,
                      OnOpacityAnimated(opacity));
}

void LayerAnimationController::notifyObserversTransformAnimated(const gfx::Transform& transform)
{
    FOR_EACH_OBSERVER(LayerAnimationValueObserver,
                      m_observers,
                      OnTransformAnimated(transform));
}

bool LayerAnimationController::hasActiveObserver()
{
    if (m_observers.might_have_observers()) {
        ObserverListBase<LayerAnimationValueObserver>::Iterator it(m_observers);
        LayerAnimationValueObserver* obs;
        while ((obs = it.GetNext()) != NULL)
            if (obs->IsActive())
                return true;
    }
    return false;
}

}  // namespace cc
