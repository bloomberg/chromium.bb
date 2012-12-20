// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_animation_controller.h"

#include "cc/active_animation.h"
#include "cc/animation_curve.h"
#include "cc/test/animation_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

void expectTranslateX(double translateX, const gfx::Transform& matrix)
{
    EXPECT_FLOAT_EQ(translateX, matrix.matrix().getDouble(0, 3));
}

scoped_ptr<ActiveAnimation> createActiveAnimation(scoped_ptr<AnimationCurve> curve, int id, ActiveAnimation::TargetProperty property)
{
    return ActiveAnimation::create(curve.Pass(), 0, id, property);
}

TEST(LayerAnimationControllerTest, syncNewAnimation)
{
    FakeLayerAnimationValueObserver dummyImpl;
    scoped_refptr<LayerAnimationController> controllerImpl(LayerAnimationController::create(0));
    controllerImpl->addObserver(&dummyImpl);
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    EXPECT_FALSE(controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity));

    addOpacityTransitionToController(*controller, 1, 0, 1, false);

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity));
    EXPECT_EQ(ActiveAnimation::WaitingForTargetAvailability, controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity)->runState());
}

// If an animation is started on the impl thread before it is ticked on the main
// thread, we must be sure to respect the synchronized start time.
TEST(LayerAnimationControllerTest, doNotClobberStartTimes)
{
    FakeLayerAnimationValueObserver dummyImpl;
    scoped_refptr<LayerAnimationController> controllerImpl(LayerAnimationController::create(0));
    controllerImpl->addObserver(&dummyImpl);
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    EXPECT_FALSE(controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity));

    addOpacityTransitionToController(*controller, 1, 0, 1, false);

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity));
    EXPECT_EQ(ActiveAnimation::WaitingForTargetAvailability, controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity)->runState());

    AnimationEventsVector events;
    controllerImpl->animate(1, &events);

    // Synchronize the start times.
    EXPECT_EQ(1u, events.size());
    controller->OnAnimationStarted(events[0]);
    EXPECT_EQ(controller->getActiveAnimation(0, ActiveAnimation::Opacity)->startTime(), controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity)->startTime());

    // Start the animation on the main thread. Should not affect the start time.
    controller->animate(1.5, 0);
    EXPECT_EQ(controller->getActiveAnimation(0, ActiveAnimation::Opacity)->startTime(), controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity)->startTime());
}

TEST(LayerAnimationControllerTest, syncPauseAndResume)
{
    FakeLayerAnimationValueObserver dummyImpl;
    scoped_refptr<LayerAnimationController> controllerImpl(LayerAnimationController::create(0));
    controllerImpl->addObserver(&dummyImpl);
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    EXPECT_FALSE(controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity));

    addOpacityTransitionToController(*controller, 1, 0, 1, false);

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity));
    EXPECT_EQ(ActiveAnimation::WaitingForTargetAvailability, controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity)->runState());

    // Start the animations on each controller.
    AnimationEventsVector events;
    controllerImpl->animate(0, &events);
    controller->animate(0, 0);
    EXPECT_EQ(ActiveAnimation::Running, controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity)->runState());
    EXPECT_EQ(ActiveAnimation::Running, controller->getActiveAnimation(0, ActiveAnimation::Opacity)->runState());

    // Pause the main-thread animation.
    controller->suspendAnimations(1);
    EXPECT_EQ(ActiveAnimation::Paused, controller->getActiveAnimation(0, ActiveAnimation::Opacity)->runState());

    // The pause run state change should make it to the impl thread controller.
    controller->pushAnimationUpdatesTo(controllerImpl.get());
    EXPECT_EQ(ActiveAnimation::Paused, controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity)->runState());

    // Resume the main-thread animation.
    controller->resumeAnimations(2);
    EXPECT_EQ(ActiveAnimation::Running, controller->getActiveAnimation(0, ActiveAnimation::Opacity)->runState());

    // The pause run state change should make it to the impl thread controller.
    controller->pushAnimationUpdatesTo(controllerImpl.get());
    EXPECT_EQ(ActiveAnimation::Running, controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity)->runState());
}

TEST(LayerAnimationControllerTest, doNotSyncFinishedAnimation)
{
    FakeLayerAnimationValueObserver dummyImpl;
    scoped_refptr<LayerAnimationController> controllerImpl(LayerAnimationController::create(0));
    controllerImpl->addObserver(&dummyImpl);
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    EXPECT_FALSE(controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity));

    int animationId = addOpacityTransitionToController(*controller, 1, 0, 1, false);

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity));
    EXPECT_EQ(ActiveAnimation::WaitingForTargetAvailability, controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity)->runState());

    // Notify main thread controller that the animation has started.
    AnimationEvent animationStartedEvent(AnimationEvent::Started, 0, 0, ActiveAnimation::Opacity, 0);
    controller->OnAnimationStarted(animationStartedEvent);

    // Force animation to complete on impl thread.
    controllerImpl->removeAnimation(animationId);

    EXPECT_FALSE(controllerImpl->getActiveAnimation(animationId, ActiveAnimation::Opacity));

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    // Even though the main thread has a 'new' animation, it should not be pushed because the animation has already completed on the impl thread.
    EXPECT_FALSE(controllerImpl->getActiveAnimation(animationId, ActiveAnimation::Opacity));
}

// Tests that transitioning opacity from 0 to 1 works as expected.
TEST(LayerAnimationControllerTest, TrivialTransition)
{
    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(new AnimationEventsVector));
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    scoped_ptr<ActiveAnimation> toAdd(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), 1, ActiveAnimation::Opacity));

    controller->addAnimation(toAdd.Pass());
    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1, events.get());
    EXPECT_EQ(1, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests animations that are waiting for a synchronized start time do not finish.
TEST(LayerAnimationControllerTest, AnimationsWaitingForStartTimeDoNotFinishIfTheyWaitLongerToStartThanTheirDuration)
{
    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(new AnimationEventsVector));
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    scoped_ptr<ActiveAnimation> toAdd(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), 1, ActiveAnimation::Opacity));
    toAdd->setNeedsSynchronizedStartTime(true);

    // We should pause at the first keyframe indefinitely waiting for that animation to start.
    controller->addAnimation(toAdd.Pass());
    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(2, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());

    // Send the synchronized start time.
    controller->OnAnimationStarted(AnimationEvent(AnimationEvent::Started, 0, 1, ActiveAnimation::Opacity, 2));
    controller->animate(5, events.get());
    EXPECT_EQ(1, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests that two queued animations affecting the same property run in sequence.
TEST(LayerAnimationControllerTest, TrivialQueuing)
{
    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(new AnimationEventsVector));
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    controller->addAnimation(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), 1, ActiveAnimation::Opacity));
    controller->addAnimation(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(1, 1, 0.5)).PassAs<AnimationCurve>(), 2, ActiveAnimation::Opacity));

    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());
    controller->animate(2, events.get());
    EXPECT_EQ(0.5, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests interrupting a transition with another transition.
TEST(LayerAnimationControllerTest, Interrupt)
{
    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(new AnimationEventsVector));
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);
    controller->addAnimation(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), 1, ActiveAnimation::Opacity));
    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());

    scoped_ptr<ActiveAnimation> toAdd(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(1, 1, 0.5)).PassAs<AnimationCurve>(), 2, ActiveAnimation::Opacity));
    toAdd->setRunState(ActiveAnimation::WaitingForNextTick, 0);
    controller->addAnimation(toAdd.Pass());

    // Since the animation was in the WaitingForNextTick state, it should start right in
    // this call to animate.
    controller->animate(0.5, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());
    controller->animate(1.5, events.get());
    EXPECT_EQ(0.5, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling two animations to run together when only one property is free.
TEST(LayerAnimationControllerTest, ScheduleTogetherWhenAPropertyIsBlocked)
{
    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(new AnimationEventsVector));
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    controller->addAnimation(createActiveAnimation(make_scoped_ptr(new FakeTransformTransition(1)).PassAs<AnimationCurve>(), 1, ActiveAnimation::Transform));
    controller->addAnimation(createActiveAnimation(make_scoped_ptr(new FakeTransformTransition(1)).PassAs<AnimationCurve>(), 2, ActiveAnimation::Transform));
    controller->addAnimation(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), 2, ActiveAnimation::Opacity));

    controller->animate(0, events.get());
    EXPECT_EQ(0, dummy.opacity());
    EXPECT_TRUE(controller->hasActiveAnimation());
    controller->animate(1, events.get());
    // Should not have started the float transition yet.
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    // The float animation should have started at time 1 and should be done.
    controller->animate(2, events.get());
    EXPECT_EQ(1, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling two animations to run together with different lengths and another
// animation queued to start when the shorter animation finishes (should wait
// for both to finish).
TEST(LayerAnimationControllerTest, ScheduleTogetherWithAnAnimWaiting)
{
    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(new AnimationEventsVector));
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    controller->addAnimation(createActiveAnimation(make_scoped_ptr(new FakeTransformTransition(2)).PassAs<AnimationCurve>(), 1, ActiveAnimation::Transform));
    controller->addAnimation(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), 1, ActiveAnimation::Opacity));
    controller->addAnimation(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(1, 1, 0.5)).PassAs<AnimationCurve>(), 2, ActiveAnimation::Opacity));

    // Animations with id 1 should both start now.
    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    // The opacity animation should have finished at time 1, but the group
    // of animations with id 1 don't finish until time 2 because of the length
    // of the transform animation.
    controller->animate(2, events.get());
    // Should not have started the float transition yet.
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());

    // The second opacity animation should start at time 2 and should be done by time 3
    controller->animate(3, events.get());
    EXPECT_EQ(0.5, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling an animation to start in the future.
TEST(LayerAnimationControllerTest, ScheduleAnimation)
{
    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(new AnimationEventsVector));
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    scoped_ptr<ActiveAnimation> toAdd(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), 1, ActiveAnimation::Opacity));
    toAdd->setRunState(ActiveAnimation::WaitingForStartTime, 0);
    toAdd->setStartTime(1);
    controller->addAnimation(toAdd.Pass());

    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(2, events.get());
    EXPECT_EQ(1, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling an animation to start in the future that's interrupting a running animation.
TEST(LayerAnimationControllerTest, ScheduledAnimationInterruptsRunningAnimation)
{
    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(new AnimationEventsVector));
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    controller->addAnimation(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(2, 0, 1)).PassAs<AnimationCurve>(), 1, ActiveAnimation::Opacity));

    scoped_ptr<ActiveAnimation> toAdd(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0.5, 0)).PassAs<AnimationCurve>(), 2, ActiveAnimation::Opacity));
    toAdd->setRunState(ActiveAnimation::WaitingForStartTime, 0);
    toAdd->setStartTime(1);
    controller->addAnimation(toAdd.Pass());

    // First 2s opacity transition should start immediately.
    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(0.5, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25, dummy.opacity());
    controller->animate(1, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5, dummy.opacity());
    controller->animate(2, events.get());
    EXPECT_EQ(0, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling an animation to start in the future that interrupts a running animation
// and there is yet another animation queued to start later.
TEST(LayerAnimationControllerTest, ScheduledAnimationInterruptsRunningAnimationWithAnimInQueue)
{
    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(new AnimationEventsVector));
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    controller->addAnimation(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(2, 0, 1)).PassAs<AnimationCurve>(), 1, ActiveAnimation::Opacity));

    scoped_ptr<ActiveAnimation> toAdd(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(2, 0.5, 0)).PassAs<AnimationCurve>(), 2, ActiveAnimation::Opacity));
    toAdd->setRunState(ActiveAnimation::WaitingForStartTime, 0);
    toAdd->setStartTime(1);
    controller->addAnimation(toAdd.Pass());

    controller->addAnimation(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 0.75)).PassAs<AnimationCurve>(), 3, ActiveAnimation::Opacity));

    // First 2s opacity transition should start immediately.
    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(0.5, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25, dummy.opacity());
    EXPECT_TRUE(controller->hasActiveAnimation());
    controller->animate(1, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5, dummy.opacity());
    controller->animate(3, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(4, events.get());
    EXPECT_EQ(0.75, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Test that a looping animation loops and for the correct number of iterations.
TEST(LayerAnimationControllerTest, TrivialLooping)
{
    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(new AnimationEventsVector));
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    scoped_ptr<ActiveAnimation> toAdd(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), 1, ActiveAnimation::Opacity));
    toAdd->setIterations(3);
    controller->addAnimation(toAdd.Pass());

    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1.25, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25, dummy.opacity());
    controller->animate(1.75, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75, dummy.opacity());
    controller->animate(2.25, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25, dummy.opacity());
    controller->animate(2.75, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75, dummy.opacity());
    controller->animate(3, events.get());
    EXPECT_FALSE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());

    // Just be extra sure.
    controller->animate(4, events.get());
    EXPECT_EQ(1, dummy.opacity());
}

// Test that an infinitely looping animation does indeed go until aborted.
TEST(LayerAnimationControllerTest, InfiniteLooping)
{
    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(new AnimationEventsVector));
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    const int id = 1;
    scoped_ptr<ActiveAnimation> toAdd(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), id, ActiveAnimation::Opacity));
    toAdd->setIterations(-1);
    controller->addAnimation(toAdd.Pass());

    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1.25, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25, dummy.opacity());
    controller->animate(1.75, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75, dummy.opacity());

    controller->animate(1073741824.25, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25, dummy.opacity());
    controller->animate(1073741824.75, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75, dummy.opacity());

    EXPECT_TRUE(controller->getActiveAnimation(id, ActiveAnimation::Opacity));
    controller->getActiveAnimation(id, ActiveAnimation::Opacity)->setRunState(ActiveAnimation::Aborted, 0.75);
    EXPECT_FALSE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75, dummy.opacity());
}

// Test that pausing and resuming work as expected.
TEST(LayerAnimationControllerTest, PauseResume)
{
    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(new AnimationEventsVector));
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    const int id = 1;
    controller->addAnimation(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), id, ActiveAnimation::Opacity));

    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(0.5, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5, dummy.opacity());

    EXPECT_TRUE(controller->getActiveAnimation(id, ActiveAnimation::Opacity));
    controller->getActiveAnimation(id, ActiveAnimation::Opacity)->setRunState(ActiveAnimation::Paused, 0.5);

    controller->animate(1024, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5, dummy.opacity());

    EXPECT_TRUE(controller->getActiveAnimation(id, ActiveAnimation::Opacity));
    controller->getActiveAnimation(id, ActiveAnimation::Opacity)->setRunState(ActiveAnimation::Running, 1024);

    controller->animate(1024.25, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75, dummy.opacity());
    controller->animate(1024.5, events.get());
    EXPECT_FALSE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());
}

TEST(LayerAnimationControllerTest, AbortAGroupedAnimation)
{
    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(new AnimationEventsVector));
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    const int id = 1;
    controller->addAnimation(createActiveAnimation(make_scoped_ptr(new FakeTransformTransition(1)).PassAs<AnimationCurve>(), id, ActiveAnimation::Transform));
    controller->addAnimation(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(2, 0, 1)).PassAs<AnimationCurve>(), id, ActiveAnimation::Opacity));
    controller->addAnimation(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(1, 1, 0.75)).PassAs<AnimationCurve>(), 2, ActiveAnimation::Opacity));

    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5, dummy.opacity());

    EXPECT_TRUE(controller->getActiveAnimation(id, ActiveAnimation::Opacity));
    controller->getActiveAnimation(id, ActiveAnimation::Opacity)->setRunState(ActiveAnimation::Aborted, 1);
    controller->animate(1, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());
    controller->animate(2, events.get());
    EXPECT_TRUE(!controller->hasActiveAnimation());
    EXPECT_EQ(0.75, dummy.opacity());
}

TEST(LayerAnimationControllerTest, ForceSyncWhenSynchronizedStartTimeNeeded)
{
    FakeLayerAnimationValueObserver dummyImpl;
    scoped_refptr<LayerAnimationController> controllerImpl(LayerAnimationController::create(0));
    controllerImpl->addObserver(&dummyImpl);
    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(new AnimationEventsVector));
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    scoped_ptr<ActiveAnimation> toAdd(createActiveAnimation(make_scoped_ptr(new FakeFloatTransition(2, 0, 1)).PassAs<AnimationCurve>(), 0, ActiveAnimation::Opacity));
    toAdd->setNeedsSynchronizedStartTime(true);
    controller->addAnimation(toAdd.Pass());

    controller->animate(0, 0);
    EXPECT_TRUE(controller->hasActiveAnimation());
    ActiveAnimation* activeAnimation = controller->getActiveAnimation(0, ActiveAnimation::Opacity);
    EXPECT_TRUE(activeAnimation);
    EXPECT_TRUE(activeAnimation->needsSynchronizedStartTime());

    controller->setForceSync();

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    activeAnimation = controllerImpl->getActiveAnimation(0, ActiveAnimation::Opacity);
    EXPECT_TRUE(activeAnimation);
    EXPECT_EQ(ActiveAnimation::WaitingForTargetAvailability, activeAnimation->runState());
}

}  // namespace
}  // namespace cc
