// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_animation_controller.h"

#include "cc/animation.h"
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

scoped_ptr<Animation> createAnimation(scoped_ptr<AnimationCurve> curve, int id, Animation::TargetProperty property)
{
    return Animation::create(curve.Pass(), 0, id, property);
}

TEST(LayerAnimationControllerTest, syncNewAnimation)
{
    FakeLayerAnimationValueObserver dummyImpl;
    scoped_refptr<LayerAnimationController> controllerImpl(LayerAnimationController::create(0));
    controllerImpl->addObserver(&dummyImpl);
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    EXPECT_FALSE(controllerImpl->getAnimation(0, Animation::Opacity));

    addOpacityTransitionToController(*controller, 1, 0, 1, false);

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getAnimation(0, Animation::Opacity));
    EXPECT_EQ(Animation::WaitingForTargetAvailability, controllerImpl->getAnimation(0, Animation::Opacity)->runState());
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

    EXPECT_FALSE(controllerImpl->getAnimation(0, Animation::Opacity));

    addOpacityTransitionToController(*controller, 1, 0, 1, false);

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getAnimation(0, Animation::Opacity));
    EXPECT_EQ(Animation::WaitingForTargetAvailability, controllerImpl->getAnimation(0, Animation::Opacity)->runState());

    AnimationEventsVector events;
    controllerImpl->animate(1, &events);

    // Synchronize the start times.
    EXPECT_EQ(1u, events.size());
    controller->OnAnimationStarted(events[0]);
    EXPECT_EQ(controller->getAnimation(0, Animation::Opacity)->startTime(), controllerImpl->getAnimation(0, Animation::Opacity)->startTime());

    // Start the animation on the main thread. Should not affect the start time.
    controller->animate(1.5, 0);
    EXPECT_EQ(controller->getAnimation(0, Animation::Opacity)->startTime(), controllerImpl->getAnimation(0, Animation::Opacity)->startTime());
}

TEST(LayerAnimationControllerTest, syncPauseAndResume)
{
    FakeLayerAnimationValueObserver dummyImpl;
    scoped_refptr<LayerAnimationController> controllerImpl(LayerAnimationController::create(0));
    controllerImpl->addObserver(&dummyImpl);
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    EXPECT_FALSE(controllerImpl->getAnimation(0, Animation::Opacity));

    addOpacityTransitionToController(*controller, 1, 0, 1, false);

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getAnimation(0, Animation::Opacity));
    EXPECT_EQ(Animation::WaitingForTargetAvailability, controllerImpl->getAnimation(0, Animation::Opacity)->runState());

    // Start the animations on each controller.
    AnimationEventsVector events;
    controllerImpl->animate(0, &events);
    controller->animate(0, 0);
    EXPECT_EQ(Animation::Running, controllerImpl->getAnimation(0, Animation::Opacity)->runState());
    EXPECT_EQ(Animation::Running, controller->getAnimation(0, Animation::Opacity)->runState());

    // Pause the main-thread animation.
    controller->suspendAnimations(1);
    EXPECT_EQ(Animation::Paused, controller->getAnimation(0, Animation::Opacity)->runState());

    // The pause run state change should make it to the impl thread controller.
    controller->pushAnimationUpdatesTo(controllerImpl.get());
    EXPECT_EQ(Animation::Paused, controllerImpl->getAnimation(0, Animation::Opacity)->runState());

    // Resume the main-thread animation.
    controller->resumeAnimations(2);
    EXPECT_EQ(Animation::Running, controller->getAnimation(0, Animation::Opacity)->runState());

    // The pause run state change should make it to the impl thread controller.
    controller->pushAnimationUpdatesTo(controllerImpl.get());
    EXPECT_EQ(Animation::Running, controllerImpl->getAnimation(0, Animation::Opacity)->runState());
}

TEST(LayerAnimationControllerTest, doNotSyncFinishedAnimation)
{
    FakeLayerAnimationValueObserver dummyImpl;
    scoped_refptr<LayerAnimationController> controllerImpl(LayerAnimationController::create(0));
    controllerImpl->addObserver(&dummyImpl);
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    EXPECT_FALSE(controllerImpl->getAnimation(0, Animation::Opacity));

    int animationId = addOpacityTransitionToController(*controller, 1, 0, 1, false);

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getAnimation(0, Animation::Opacity));
    EXPECT_EQ(Animation::WaitingForTargetAvailability, controllerImpl->getAnimation(0, Animation::Opacity)->runState());

    // Notify main thread controller that the animation has started.
    AnimationEvent animationStartedEvent(AnimationEvent::Started, 0, 0, Animation::Opacity, 0);
    controller->OnAnimationStarted(animationStartedEvent);

    // Force animation to complete on impl thread.
    controllerImpl->removeAnimation(animationId);

    EXPECT_FALSE(controllerImpl->getAnimation(animationId, Animation::Opacity));

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    // Even though the main thread has a 'new' animation, it should not be pushed because the animation has already completed on the impl thread.
    EXPECT_FALSE(controllerImpl->getAnimation(animationId, Animation::Opacity));
}

// Tests that transitioning opacity from 0 to 1 works as expected.
TEST(LayerAnimationControllerTest, TrivialTransition)
{
    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(new AnimationEventsVector));
    FakeLayerAnimationValueObserver dummy;
    scoped_refptr<LayerAnimationController> controller(LayerAnimationController::create(0));
    controller->addObserver(&dummy);

    scoped_ptr<Animation> toAdd(createAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), 1, Animation::Opacity));

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

    scoped_ptr<Animation> toAdd(createAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), 1, Animation::Opacity));
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
    controller->OnAnimationStarted(AnimationEvent(AnimationEvent::Started, 0, 1, Animation::Opacity, 2));
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

    controller->addAnimation(createAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), 1, Animation::Opacity));
    controller->addAnimation(createAnimation(make_scoped_ptr(new FakeFloatTransition(1, 1, 0.5)).PassAs<AnimationCurve>(), 2, Animation::Opacity));

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
    controller->addAnimation(createAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), 1, Animation::Opacity));
    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());

    scoped_ptr<Animation> toAdd(createAnimation(make_scoped_ptr(new FakeFloatTransition(1, 1, 0.5)).PassAs<AnimationCurve>(), 2, Animation::Opacity));
    toAdd->setRunState(Animation::WaitingForNextTick, 0);
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

    controller->addAnimation(createAnimation(make_scoped_ptr(new FakeTransformTransition(1)).PassAs<AnimationCurve>(), 1, Animation::Transform));
    controller->addAnimation(createAnimation(make_scoped_ptr(new FakeTransformTransition(1)).PassAs<AnimationCurve>(), 2, Animation::Transform));
    controller->addAnimation(createAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), 2, Animation::Opacity));

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

    controller->addAnimation(createAnimation(make_scoped_ptr(new FakeTransformTransition(2)).PassAs<AnimationCurve>(), 1, Animation::Transform));
    controller->addAnimation(createAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), 1, Animation::Opacity));
    controller->addAnimation(createAnimation(make_scoped_ptr(new FakeFloatTransition(1, 1, 0.5)).PassAs<AnimationCurve>(), 2, Animation::Opacity));

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

    scoped_ptr<Animation> toAdd(createAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), 1, Animation::Opacity));
    toAdd->setRunState(Animation::WaitingForStartTime, 0);
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

    controller->addAnimation(createAnimation(make_scoped_ptr(new FakeFloatTransition(2, 0, 1)).PassAs<AnimationCurve>(), 1, Animation::Opacity));

    scoped_ptr<Animation> toAdd(createAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0.5, 0)).PassAs<AnimationCurve>(), 2, Animation::Opacity));
    toAdd->setRunState(Animation::WaitingForStartTime, 0);
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

    controller->addAnimation(createAnimation(make_scoped_ptr(new FakeFloatTransition(2, 0, 1)).PassAs<AnimationCurve>(), 1, Animation::Opacity));

    scoped_ptr<Animation> toAdd(createAnimation(make_scoped_ptr(new FakeFloatTransition(2, 0.5, 0)).PassAs<AnimationCurve>(), 2, Animation::Opacity));
    toAdd->setRunState(Animation::WaitingForStartTime, 0);
    toAdd->setStartTime(1);
    controller->addAnimation(toAdd.Pass());

    controller->addAnimation(createAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 0.75)).PassAs<AnimationCurve>(), 3, Animation::Opacity));

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

    scoped_ptr<Animation> toAdd(createAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), 1, Animation::Opacity));
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
    scoped_ptr<Animation> toAdd(createAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), id, Animation::Opacity));
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

    EXPECT_TRUE(controller->getAnimation(id, Animation::Opacity));
    controller->getAnimation(id, Animation::Opacity)->setRunState(Animation::Aborted, 0.75);
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
    controller->addAnimation(createAnimation(make_scoped_ptr(new FakeFloatTransition(1, 0, 1)).PassAs<AnimationCurve>(), id, Animation::Opacity));

    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(0.5, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5, dummy.opacity());

    EXPECT_TRUE(controller->getAnimation(id, Animation::Opacity));
    controller->getAnimation(id, Animation::Opacity)->setRunState(Animation::Paused, 0.5);

    controller->animate(1024, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5, dummy.opacity());

    EXPECT_TRUE(controller->getAnimation(id, Animation::Opacity));
    controller->getAnimation(id, Animation::Opacity)->setRunState(Animation::Running, 1024);

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
    controller->addAnimation(createAnimation(make_scoped_ptr(new FakeTransformTransition(1)).PassAs<AnimationCurve>(), id, Animation::Transform));
    controller->addAnimation(createAnimation(make_scoped_ptr(new FakeFloatTransition(2, 0, 1)).PassAs<AnimationCurve>(), id, Animation::Opacity));
    controller->addAnimation(createAnimation(make_scoped_ptr(new FakeFloatTransition(1, 1, 0.75)).PassAs<AnimationCurve>(), 2, Animation::Opacity));

    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5, dummy.opacity());

    EXPECT_TRUE(controller->getAnimation(id, Animation::Opacity));
    controller->getAnimation(id, Animation::Opacity)->setRunState(Animation::Aborted, 1);
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

    scoped_ptr<Animation> toAdd(createAnimation(make_scoped_ptr(new FakeFloatTransition(2, 0, 1)).PassAs<AnimationCurve>(), 0, Animation::Opacity));
    toAdd->setNeedsSynchronizedStartTime(true);
    controller->addAnimation(toAdd.Pass());

    controller->animate(0, 0);
    EXPECT_TRUE(controller->hasActiveAnimation());
    Animation* activeAnimation = controller->getAnimation(0, Animation::Opacity);
    EXPECT_TRUE(activeAnimation);
    EXPECT_TRUE(activeAnimation->needsSynchronizedStartTime());

    controller->setForceSync();

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    activeAnimation = controllerImpl->getAnimation(0, Animation::Opacity);
    EXPECT_TRUE(activeAnimation);
    EXPECT_EQ(Animation::WaitingForTargetAvailability, activeAnimation->runState());
}

}  // namespace
}  // namespace cc
