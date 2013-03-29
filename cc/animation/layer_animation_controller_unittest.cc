// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/layer_animation_controller.h"

#include "cc/animation/animation.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/transform_operations.h"
#include "cc/test/animation_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

void ExpectTranslateX(double translate_x, const gfx::Transform& matrix) {
  EXPECT_FLOAT_EQ(translate_x, matrix.matrix().getDouble(0, 3)); }

scoped_ptr<Animation> CreateAnimation(scoped_ptr<AnimationCurve> curve,
                                      int id,
                                      Animation::TargetProperty property) {
  return Animation::Create(curve.Pass(), 0, id, property);
}

TEST(LayerAnimationControllerTest, SyncNewAnimation) {
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddObserver(&dummy_impl);
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  EXPECT_FALSE(controller_impl->GetAnimation(0, Animation::Opacity));

  AddOpacityTransitionToController(controller, 1, 0, 1, false);

  controller->PushAnimationUpdatesTo(controller_impl.get());

  EXPECT_TRUE(controller_impl->GetAnimation(0, Animation::Opacity));
  EXPECT_EQ(Animation::WaitingForTargetAvailability,
            controller_impl->GetAnimation(0, Animation::Opacity)->run_state());
}

// If an animation is started on the impl thread before it is ticked on the main
// thread, we must be sure to respect the synchronized start time.
TEST(LayerAnimationControllerTest, DoNotClobberStartTimes) {
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddObserver(&dummy_impl);
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  EXPECT_FALSE(controller_impl->GetAnimation(0, Animation::Opacity));

  AddOpacityTransitionToController(controller, 1, 0, 1, false);

  controller->PushAnimationUpdatesTo(controller_impl.get());

  EXPECT_TRUE(controller_impl->GetAnimation(0, Animation::Opacity));
  EXPECT_EQ(Animation::WaitingForTargetAvailability,
            controller_impl->GetAnimation(0, Animation::Opacity)->run_state());

  AnimationEventsVector events;
  controller_impl->Animate(1.0);
  controller_impl->UpdateState(&events);

  // Synchronize the start times.
  EXPECT_EQ(1u, events.size());
  controller->OnAnimationStarted(events[0]);
  EXPECT_EQ(controller->GetAnimation(0, Animation::Opacity)->start_time(),
            controller_impl->GetAnimation(0, Animation::Opacity)->start_time());

  // Start the animation on the main thread. Should not affect the start time.
  controller->Animate(1.5);
  controller->UpdateState(NULL);
  EXPECT_EQ(controller->GetAnimation(0, Animation::Opacity)->start_time(),
            controller_impl->GetAnimation(0, Animation::Opacity)->start_time());
}

TEST(LayerAnimationControllerTest, SyncPauseAndResume) {
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddObserver(&dummy_impl);
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  EXPECT_FALSE(controller_impl->GetAnimation(0, Animation::Opacity));

  AddOpacityTransitionToController(controller, 1, 0, 1, false);

  controller->PushAnimationUpdatesTo(controller_impl.get());

  EXPECT_TRUE(controller_impl->GetAnimation(0, Animation::Opacity));
  EXPECT_EQ(Animation::WaitingForTargetAvailability,
            controller_impl->GetAnimation(0, Animation::Opacity)->run_state());

  // Start the animations on each controller.
  AnimationEventsVector events;
  controller_impl->Animate(0.0);
  controller_impl->UpdateState(&events);
  controller->Animate(0.0);
  controller->UpdateState(NULL);
  EXPECT_EQ(Animation::Running,
            controller_impl->GetAnimation(0, Animation::Opacity)->run_state());
  EXPECT_EQ(Animation::Running,
            controller->GetAnimation(0, Animation::Opacity)->run_state());

  // Pause the main-thread animation.
  controller->SuspendAnimations(1.0);
  EXPECT_EQ(Animation::Paused,
            controller->GetAnimation(0, Animation::Opacity)->run_state());

  // The pause run state change should make it to the impl thread controller.
  controller->PushAnimationUpdatesTo(controller_impl.get());
  EXPECT_EQ(Animation::Paused,
            controller_impl->GetAnimation(0, Animation::Opacity)->run_state());

  // Resume the main-thread animation.
  controller->ResumeAnimations(2.0);
  EXPECT_EQ(Animation::Running,
            controller->GetAnimation(0, Animation::Opacity)->run_state());

  // The pause run state change should make it to the impl thread controller.
  controller->PushAnimationUpdatesTo(controller_impl.get());
  EXPECT_EQ(Animation::Running,
            controller_impl->GetAnimation(0, Animation::Opacity)->run_state());
}

TEST(LayerAnimationControllerTest, DoNotSyncFinishedAnimation) {
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddObserver(&dummy_impl);
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  EXPECT_FALSE(controller_impl->GetAnimation(0, Animation::Opacity));

  int animation_id =
      AddOpacityTransitionToController(controller, 1, 0, 1, false);

  controller->PushAnimationUpdatesTo(controller_impl.get());

  EXPECT_TRUE(controller_impl->GetAnimation(0, Animation::Opacity));
  EXPECT_EQ(Animation::WaitingForTargetAvailability,
            controller_impl->GetAnimation(0, Animation::Opacity)->run_state());

  // Notify main thread controller that the animation has started.
  AnimationEvent animation_started_event(
      AnimationEvent::Started, 0, 0, Animation::Opacity, 0);
  controller->OnAnimationStarted(animation_started_event);

  // Force animation to complete on impl thread.
  controller_impl->RemoveAnimation(animation_id);

  EXPECT_FALSE(controller_impl->GetAnimation(animation_id, Animation::Opacity));

  controller->PushAnimationUpdatesTo(controller_impl.get());

  // Even though the main thread has a 'new' animation, it should not be pushed
  // because the animation has already completed on the impl thread.
  EXPECT_FALSE(controller_impl->GetAnimation(animation_id, Animation::Opacity));
}

// Tests that transitioning opacity from 0 to 1 works as expected.

static const AnimationEvent* GetMostRecentPropertyUpdateEvent(
    const AnimationEventsVector* events) {
  const AnimationEvent* event = 0;
  for (size_t i = 0; i < events->size(); ++i)
    if ((*events)[i].type == AnimationEvent::PropertyUpdate)
      event = &(*events)[i];

  return event;
}

TEST(LayerAnimationControllerTest, TrivialTransition) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));

  controller->AddAnimation(to_add.Pass());
  controller->Animate(0.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  // A non-impl-only animation should not generate property updates.
  const AnimationEvent* event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);
  controller->Animate(1.0);
  controller->UpdateState(events.get());
  EXPECT_EQ(1.f, dummy.opacity());
  EXPECT_FALSE(controller->HasActiveAnimation());
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);
}

TEST(LayerAnimationControllerTest, TrivialTransitionOnImpl) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddObserver(&dummy_impl);

  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));
  to_add->set_is_impl_only(true);

  controller_impl->AddAnimation(to_add.Pass());
  controller_impl->Animate(0.0);
  controller_impl->UpdateState(events.get());
  EXPECT_TRUE(controller_impl->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy_impl.opacity());
  EXPECT_EQ(2, events->size());
  const AnimationEvent* start_opacity_event =
      GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_EQ(0, start_opacity_event->opacity);

  controller_impl->Animate(1.0);
  controller_impl->UpdateState(events.get());
  EXPECT_EQ(1.f, dummy_impl.opacity());
  EXPECT_FALSE(controller_impl->HasActiveAnimation());
  EXPECT_EQ(4, events->size());
  const AnimationEvent* end_opacity_event =
      GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_EQ(1, end_opacity_event->opacity);
}

TEST(LayerAnimationControllerTest, TrivialTransformOnImpl) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddObserver(&dummy_impl);

  // Choose different values for x and y to avoid coincidental values in the
  // observed transforms.
  const float delta_x = 3;
  const float delta_y = 4;

  scoped_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());

  // Create simple Transform animation.
  TransformOperations operations;
  curve->AddKeyframe(TransformKeyframe::Create(
      0, operations, scoped_ptr<cc::TimingFunction>()));
  operations.AppendTranslate(delta_x, delta_y, 0);
  curve->AddKeyframe(TransformKeyframe::Create(
      1, operations, scoped_ptr<cc::TimingFunction>()));

  scoped_ptr<Animation> animation(Animation::Create(
      curve.PassAs<AnimationCurve>(), 1, 0, Animation::Transform));
  animation->set_is_impl_only(true);
  controller_impl->AddAnimation(animation.Pass());

  // Run animation.
  controller_impl->Animate(0.0);
  controller_impl->UpdateState(events.get());
  EXPECT_TRUE(controller_impl->HasActiveAnimation());
  EXPECT_EQ(gfx::Transform(), dummy_impl.transform());
  EXPECT_EQ(2, events->size());
  const AnimationEvent* start_transform_event =
      GetMostRecentPropertyUpdateEvent(events.get());
  ASSERT_TRUE(start_transform_event);
  EXPECT_EQ(gfx::Transform(), start_transform_event->transform);

  gfx::Transform expected_transform;
  expected_transform.Translate(delta_x, delta_y);

  controller_impl->Animate(1.0);
  controller_impl->UpdateState(events.get());
  EXPECT_EQ(expected_transform, dummy_impl.transform());
  EXPECT_FALSE(controller_impl->HasActiveAnimation());
  EXPECT_EQ(4, events->size());
  const AnimationEvent* end_transform_event =
      GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_EQ(expected_transform, end_transform_event->transform);
}

// Tests animations that are waiting for a synchronized start time do not
// finish.
TEST(LayerAnimationControllerTest,
     AnimationsWaitingForStartTimeDoNotFinishIfTheyWaitLongerToStartThanTheirDuration) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));
  to_add->set_needs_synchronized_start_time(true);

  // We should pause at the first keyframe indefinitely waiting for that
  // animation to start.
  controller->AddAnimation(to_add.Pass());
  controller->Animate(0.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(1.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(2.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());

  // Send the synchronized start time.
  controller->OnAnimationStarted(AnimationEvent(
      AnimationEvent::Started, 0, 1, Animation::Opacity, 2));
  controller->Animate(5.0);
  controller->UpdateState(events.get());
  EXPECT_EQ(1.f, dummy.opacity());
  EXPECT_FALSE(controller->HasActiveAnimation());
}

// Tests that two queued animations affecting the same property run in sequence.
TEST(LayerAnimationControllerTest, TrivialQueuing) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(
          new FakeFloatTransition(1.0, 1.f, 0.5f)).Pass(),
      2,
      Animation::Opacity));

  controller->Animate(0.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(1.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(1.f, dummy.opacity());
  controller->Animate(2.0);
  controller->UpdateState(events.get());
  EXPECT_EQ(0.5f, dummy.opacity());
  EXPECT_FALSE(controller->HasActiveAnimation());
}

// Tests interrupting a transition with another transition.
TEST(LayerAnimationControllerTest, Interrupt) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));
  controller->Animate(0.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());

  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(
          new FakeFloatTransition(1.0, 1.f, 0.5f)).Pass(),
      2,
      Animation::Opacity));
  to_add->SetRunState(Animation::WaitingForNextTick, 0);
  controller->AddAnimation(to_add.Pass());

  // Since the animation was in the WaitingForNextTick state, it should start
  // right in this call to animate.
  controller->Animate(0.5);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(1.f, dummy.opacity());
  controller->Animate(1.5);
  controller->UpdateState(events.get());
  EXPECT_EQ(0.5f, dummy.opacity());
  EXPECT_FALSE(controller->HasActiveAnimation());
}

// Tests scheduling two animations to run together when only one property is
// free.
TEST(LayerAnimationControllerTest, ScheduleTogetherWhenAPropertyIsBlocked) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeTransformTransition(1)).Pass(),
      1,
      Animation::Transform));
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeTransformTransition(1)).Pass(),
      2,
      Animation::Transform));
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      2,
      Animation::Opacity));

  controller->Animate(0.0);
  controller->UpdateState(events.get());
  EXPECT_EQ(0.f, dummy.opacity());
  EXPECT_TRUE(controller->HasActiveAnimation());
  controller->Animate(1.0);
  controller->UpdateState(events.get());
  // Should not have started the float transition yet.
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  // The float animation should have started at time 1 and should be done.
  controller->Animate(2.0);
  controller->UpdateState(events.get());
  EXPECT_EQ(1.f, dummy.opacity());
  EXPECT_FALSE(controller->HasActiveAnimation());
}

// Tests scheduling two animations to run together with different lengths and
// another animation queued to start when the shorter animation finishes (should
// wait for both to finish).
TEST(LayerAnimationControllerTest, ScheduleTogetherWithAnAnimWaiting) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeTransformTransition(2)).Pass(),
      1,
      Animation::Transform));
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(
          new FakeFloatTransition(1.0, 1.f, 0.5f)).Pass(),
      2,
      Animation::Opacity));

  // Animations with id 1 should both start now.
  controller->Animate(0.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  // The opacity animation should have finished at time 1, but the group
  // of animations with id 1 don't finish until time 2 because of the length
  // of the transform animation.
  controller->Animate(2.0);
  controller->UpdateState(events.get());
  // Should not have started the float transition yet.
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(1.f, dummy.opacity());

  // The second opacity animation should start at time 2 and should be done by
  // time 3.
  controller->Animate(3.0);
  controller->UpdateState(events.get());
  EXPECT_EQ(0.5f, dummy.opacity());
  EXPECT_FALSE(controller->HasActiveAnimation());
}

// Tests scheduling an animation to start in the future.
TEST(LayerAnimationControllerTest, ScheduleAnimation) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));
  to_add->SetRunState(Animation::WaitingForStartTime, 0);
  to_add->set_start_time(1.f);
  controller->AddAnimation(to_add.Pass());

  controller->Animate(0.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(1.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(2.0);
  controller->UpdateState(events.get());
  EXPECT_EQ(1.f, dummy.opacity());
  EXPECT_FALSE(controller->HasActiveAnimation());
}

// Tests scheduling an animation to start in the future that's interrupting a
// running animation.
TEST(LayerAnimationControllerTest,
     ScheduledAnimationInterruptsRunningAnimation) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(2.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));

  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(
          new FakeFloatTransition(1.0, 0.5f, 0.f)).Pass(),
      2,
      Animation::Opacity));
  to_add->SetRunState(Animation::WaitingForStartTime, 0);
  to_add->set_start_time(1.f);
  controller->AddAnimation(to_add.Pass());

  // First 2s opacity transition should start immediately.
  controller->Animate(0.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(0.5);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.25f, dummy.opacity());
  controller->Animate(1.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.5f, dummy.opacity());
  controller->Animate(2.0);
  controller->UpdateState(events.get());
  EXPECT_EQ(0.f, dummy.opacity());
  EXPECT_FALSE(controller->HasActiveAnimation());
}

// Tests scheduling an animation to start in the future that interrupts a
// running animation and there is yet another animation queued to start later.
TEST(LayerAnimationControllerTest,
     ScheduledAnimationInterruptsRunningAnimationWithAnimInQueue) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(2.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));

  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(
          new FakeFloatTransition(2.0, 0.5f, 0.f)).Pass(),
      2,
      Animation::Opacity));
  to_add->SetRunState(Animation::WaitingForStartTime, 0);
  to_add->set_start_time(1.f);
  controller->AddAnimation(to_add.Pass());

  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(
          new FakeFloatTransition(1.0, 0.f, 0.75f)).Pass(),
      3,
      Animation::Opacity));

  // First 2s opacity transition should start immediately.
  controller->Animate(0.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(0.5);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.25f, dummy.opacity());
  EXPECT_TRUE(controller->HasActiveAnimation());
  controller->Animate(1.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.5f, dummy.opacity());
  controller->Animate(3.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(4.0);
  controller->UpdateState(events.get());
  EXPECT_EQ(0.75f, dummy.opacity());
  EXPECT_FALSE(controller->HasActiveAnimation());
}

// Test that a looping animation loops and for the correct number of iterations.
TEST(LayerAnimationControllerTest, TrivialLooping) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));
  to_add->set_iterations(3);
  controller->AddAnimation(to_add.Pass());

  controller->Animate(0.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(1.25);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.25f, dummy.opacity());
  controller->Animate(1.75);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.75f, dummy.opacity());
  controller->Animate(2.25);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.25f, dummy.opacity());
  controller->Animate(2.75);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.75f, dummy.opacity());
  controller->Animate(3.0);
  controller->UpdateState(events.get());
  EXPECT_FALSE(controller->HasActiveAnimation());
  EXPECT_EQ(1.f, dummy.opacity());

  // Just be extra sure.
  controller->Animate(4.0);
  controller->UpdateState(events.get());
  EXPECT_EQ(1.f, dummy.opacity());
}

// Test that an infinitely looping animation does indeed go until aborted.
TEST(LayerAnimationControllerTest, InfiniteLooping) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  const int id = 1;
  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      id,
      Animation::Opacity));
  to_add->set_iterations(-1);
  controller->AddAnimation(to_add.Pass());

  controller->Animate(0.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(1.25);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.25f, dummy.opacity());
  controller->Animate(1.75);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.75f, dummy.opacity());

  controller->Animate(1073741824.25);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.25f, dummy.opacity());
  controller->Animate(1073741824.75);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.75f, dummy.opacity());

  EXPECT_TRUE(controller->GetAnimation(id, Animation::Opacity));
  controller->GetAnimation(id, Animation::Opacity)->SetRunState(
      Animation::Aborted, 0.75);
  EXPECT_FALSE(controller->HasActiveAnimation());
  EXPECT_EQ(0.75f, dummy.opacity());
}

// Test that pausing and resuming work as expected.
TEST(LayerAnimationControllerTest, PauseResume) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  const int id = 1;
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      id,
      Animation::Opacity));

  controller->Animate(0.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(0.5);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.5f, dummy.opacity());

  EXPECT_TRUE(controller->GetAnimation(id, Animation::Opacity));
  controller->GetAnimation(id, Animation::Opacity)->SetRunState(
      Animation::Paused, 0.5);

  controller->Animate(1024);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.5f, dummy.opacity());

  EXPECT_TRUE(controller->GetAnimation(id, Animation::Opacity));
  controller->GetAnimation(id, Animation::Opacity)->SetRunState(
      Animation::Running, 1024);

  controller->Animate(1024.25);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.75f, dummy.opacity());
  controller->Animate(1024.5);
  controller->UpdateState(events.get());
  EXPECT_FALSE(controller->HasActiveAnimation());
  EXPECT_EQ(1.f, dummy.opacity());
}

TEST(LayerAnimationControllerTest, AbortAGroupedAnimation) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  const int id = 1;
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeTransformTransition(1)).Pass(),
      id,
      Animation::Transform));
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(2.0, 0.f, 1.f)).Pass(),
      id,
      Animation::Opacity));
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(
          new FakeFloatTransition(1.0, 1.f, 0.75f)).Pass(),
      2,
      Animation::Opacity));

  controller->Animate(0.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(1.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.5f, dummy.opacity());

  EXPECT_TRUE(controller->GetAnimation(id, Animation::Opacity));
  controller->GetAnimation(id, Animation::Opacity)->SetRunState(
      Animation::Aborted, 1);
  controller->Animate(1.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(1.f, dummy.opacity());
  controller->Animate(2.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(!controller->HasActiveAnimation());
  EXPECT_EQ(0.75f, dummy.opacity());
}

TEST(LayerAnimationControllerTest, ForceSyncWhenSynchronizedStartTimeNeeded) {
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddObserver(&dummy_impl);
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(2.0, 0.f, 1.f)).Pass(),
      0,
      Animation::Opacity));
  to_add->set_needs_synchronized_start_time(true);
  controller->AddAnimation(to_add.Pass());

  controller->Animate(0.0);
  controller->UpdateState(events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  Animation* active_animation = controller->GetAnimation(0, Animation::Opacity);
  EXPECT_TRUE(active_animation);
  EXPECT_TRUE(active_animation->needs_synchronized_start_time());

  controller->set_force_sync();

  controller->PushAnimationUpdatesTo(controller_impl.get());

  active_animation = controller_impl->GetAnimation(0, Animation::Opacity);
  EXPECT_TRUE(active_animation);
  EXPECT_EQ(Animation::WaitingForTargetAvailability,
            active_animation->run_state());
}

// Tests that skipping a call to UpdateState works as expected.
TEST(LayerAnimationControllerTest, SkipUpdateState) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddObserver(&dummy);

  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeTransformTransition(1)).Pass(),
      1,
      Animation::Transform));

  controller->Animate(0.0);
  controller->UpdateState(events.get());

  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      2,
      Animation::Opacity));

  // Animate but don't UpdateState.
  controller->Animate(1.0);

  controller->Animate(2.0);
  events.reset(new AnimationEventsVector);
  controller->UpdateState(events.get());

  // Should have one Started event and one Finished event.
  EXPECT_EQ(2, events->size());
  EXPECT_NE((*events)[0].type, (*events)[1].type);

  // The float transition should still be at its starting point.
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());

  controller->Animate(3.0);
  controller->UpdateState(events.get());

  // The float tranisition should now be done.
  EXPECT_EQ(1.f, dummy.opacity());
  EXPECT_FALSE(controller->HasActiveAnimation());
}

}  // namespace
}  // namespace cc
