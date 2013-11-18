// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/layer_animation_controller.h"

#include "cc/animation/animation.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_registrar.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/transform_operations.h"
#include "cc/test/animation_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/box_f.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

scoped_ptr<Animation> CreateAnimation(scoped_ptr<AnimationCurve> curve,
                                      int id,
                                      Animation::TargetProperty property) {
  return Animation::Create(curve.Pass(), 0, id, property);
}

TEST(LayerAnimationControllerTest, SyncNewAnimation) {
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddValueObserver(&dummy_impl);
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddValueObserver(&dummy);

  EXPECT_FALSE(controller_impl->GetAnimation(Animation::Opacity));

  AddOpacityTransitionToController(controller.get(), 1, 0, 1, false);
  int group_id = controller->GetAnimation(Animation::Opacity)->group();

  controller->PushAnimationUpdatesTo(controller_impl.get());

  EXPECT_TRUE(controller_impl->GetAnimation(group_id, Animation::Opacity));
  EXPECT_EQ(Animation::WaitingForTargetAvailability,
            controller_impl->GetAnimation(group_id,
                                          Animation::Opacity)->run_state());
}

// If an animation is started on the impl thread before it is ticked on the main
// thread, we must be sure to respect the synchronized start time.
TEST(LayerAnimationControllerTest, DoNotClobberStartTimes) {
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddValueObserver(&dummy_impl);
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddValueObserver(&dummy);

  EXPECT_FALSE(controller_impl->GetAnimation(Animation::Opacity));

  AddOpacityTransitionToController(controller.get(), 1, 0, 1, false);
  int group_id = controller->GetAnimation(Animation::Opacity)->group();

  controller->PushAnimationUpdatesTo(controller_impl.get());

  EXPECT_TRUE(controller_impl->GetAnimation(group_id, Animation::Opacity));
  EXPECT_EQ(Animation::WaitingForTargetAvailability,
            controller_impl->GetAnimation(group_id,
                                          Animation::Opacity)->run_state());

  AnimationEventsVector events;
  controller_impl->Animate(1.0);
  controller_impl->UpdateState(true, &events);

  // Synchronize the start times.
  EXPECT_EQ(1u, events.size());
  controller->NotifyAnimationStarted(events[0], 0.0);
  EXPECT_EQ(controller->GetAnimation(group_id,
                                     Animation::Opacity)->start_time(),
            controller_impl->GetAnimation(group_id,
                                          Animation::Opacity)->start_time());

  // Start the animation on the main thread. Should not affect the start time.
  controller->Animate(1.5);
  controller->UpdateState(true, NULL);
  EXPECT_EQ(controller->GetAnimation(group_id,
                                     Animation::Opacity)->start_time(),
            controller_impl->GetAnimation(group_id,
                                          Animation::Opacity)->start_time());
}

// Tests that controllers activate and deactivate as expected.
TEST(LayerAnimationControllerTest, Activation) {
  scoped_ptr<AnimationRegistrar> registrar = AnimationRegistrar::Create();
  scoped_ptr<AnimationRegistrar> registrar_impl = AnimationRegistrar::Create();

  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddValueObserver(&dummy_impl);
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddValueObserver(&dummy);
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));

  controller->SetAnimationRegistrar(registrar.get());
  controller_impl->SetAnimationRegistrar(registrar_impl.get());
  EXPECT_EQ(1u, registrar->all_animation_controllers().size());
  EXPECT_EQ(1u, registrar_impl->all_animation_controllers().size());

  // Initially, both controllers should be inactive.
  EXPECT_EQ(0u, registrar->active_animation_controllers().size());
  EXPECT_EQ(0u, registrar_impl->active_animation_controllers().size());

  AddOpacityTransitionToController(controller.get(), 1, 0, 1, false);
  // The main thread controller should now be active.
  EXPECT_EQ(1u, registrar->active_animation_controllers().size());

  controller->PushAnimationUpdatesTo(controller_impl.get());
  // Both controllers should now be active.
  EXPECT_EQ(1u, registrar->active_animation_controllers().size());
  EXPECT_EQ(1u, registrar_impl->active_animation_controllers().size());

  controller_impl->Animate(1.0);
  controller_impl->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->size());
  controller->NotifyAnimationStarted((*events)[0], 0.0);

  EXPECT_EQ(1u, registrar->active_animation_controllers().size());
  EXPECT_EQ(1u, registrar_impl->active_animation_controllers().size());

  controller->Animate(1.5);
  controller->UpdateState(true, NULL);
  EXPECT_EQ(1u, registrar->active_animation_controllers().size());

  controller->Animate(2.0);
  controller->UpdateState(true, NULL);
  EXPECT_EQ(Animation::Finished,
            controller->GetAnimation(Animation::Opacity)->run_state());
  EXPECT_EQ(1u, registrar->active_animation_controllers().size());

  events.reset(new AnimationEventsVector);
  controller_impl->Animate(2.5);
  controller_impl->UpdateState(true, events.get());

  EXPECT_EQ(Animation::WaitingForDeletion,
            controller_impl->GetAnimation(Animation::Opacity)->run_state());
  // The impl thread controller should have de-activated.
  EXPECT_EQ(0u, registrar_impl->active_animation_controllers().size());

  EXPECT_EQ(1u, events->size());
  controller->NotifyAnimationFinished((*events)[0], 0.0);
  controller->Animate(2.5);
  controller->UpdateState(true, NULL);

  EXPECT_EQ(Animation::WaitingForDeletion,
            controller->GetAnimation(Animation::Opacity)->run_state());
  // The main thread controller should have de-activated.
  EXPECT_EQ(0u, registrar->active_animation_controllers().size());

  controller->PushAnimationUpdatesTo(controller_impl.get());
  EXPECT_FALSE(controller->has_any_animation());
  EXPECT_FALSE(controller_impl->has_any_animation());
  EXPECT_EQ(0u, registrar->active_animation_controllers().size());
  EXPECT_EQ(0u, registrar_impl->active_animation_controllers().size());

  controller->SetAnimationRegistrar(NULL);
  controller_impl->SetAnimationRegistrar(NULL);
}

TEST(LayerAnimationControllerTest, SyncPause) {
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddValueObserver(&dummy_impl);
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddValueObserver(&dummy);

  EXPECT_FALSE(controller_impl->GetAnimation(Animation::Opacity));

  AddOpacityTransitionToController(controller.get(), 1, 0, 1, false);
  int group_id = controller->GetAnimation(Animation::Opacity)->group();
  int animation_id = controller->GetAnimation(Animation::Opacity)->id();

  controller->PushAnimationUpdatesTo(controller_impl.get());

  EXPECT_TRUE(controller_impl->GetAnimation(group_id, Animation::Opacity));
  EXPECT_EQ(Animation::WaitingForTargetAvailability,
            controller_impl->GetAnimation(group_id,
                                          Animation::Opacity)->run_state());

  // Start the animations on each controller.
  AnimationEventsVector events;
  controller_impl->Animate(0.0);
  controller_impl->UpdateState(true, &events);
  controller->Animate(0.0);
  controller->UpdateState(true, NULL);
  EXPECT_EQ(Animation::Running,
            controller_impl->GetAnimation(group_id,
                                          Animation::Opacity)->run_state());
  EXPECT_EQ(Animation::Running,
            controller->GetAnimation(group_id,
                                     Animation::Opacity)->run_state());

  // Pause the main-thread animation.
  controller->PauseAnimation(animation_id, 1.0);
  EXPECT_EQ(Animation::Paused,
            controller->GetAnimation(group_id,
                                     Animation::Opacity)->run_state());

  // The pause run state change should make it to the impl thread controller.
  controller->PushAnimationUpdatesTo(controller_impl.get());
  EXPECT_EQ(Animation::Paused,
            controller_impl->GetAnimation(group_id,
                                          Animation::Opacity)->run_state());
}

TEST(LayerAnimationControllerTest, DoNotSyncFinishedAnimation) {
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddValueObserver(&dummy_impl);
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddValueObserver(&dummy);

  EXPECT_FALSE(controller_impl->GetAnimation(Animation::Opacity));

  int animation_id =
      AddOpacityTransitionToController(controller.get(), 1, 0, 1, false);
  int group_id = controller->GetAnimation(Animation::Opacity)->group();

  controller->PushAnimationUpdatesTo(controller_impl.get());

  EXPECT_TRUE(controller_impl->GetAnimation(group_id, Animation::Opacity));
  EXPECT_EQ(Animation::WaitingForTargetAvailability,
            controller_impl->GetAnimation(group_id,
                                          Animation::Opacity)->run_state());

  // Notify main thread controller that the animation has started.
  AnimationEvent animation_started_event(
      AnimationEvent::Started, 0, group_id, Animation::Opacity, 0);
  controller->NotifyAnimationStarted(animation_started_event, 0.0);

  // Force animation to complete on impl thread.
  controller_impl->RemoveAnimation(animation_id);

  EXPECT_FALSE(controller_impl->GetAnimation(group_id, Animation::Opacity));

  controller->PushAnimationUpdatesTo(controller_impl.get());

  // Even though the main thread has a 'new' animation, it should not be pushed
  // because the animation has already completed on the impl thread.
  EXPECT_FALSE(controller_impl->GetAnimation(group_id, Animation::Opacity));
}

// Ensure that a finished animation is eventually deleted by both the
// main-thread and the impl-thread controllers.
TEST(LayerAnimationControllerTest, AnimationsAreDeleted) {
  FakeLayerAnimationValueObserver dummy;
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller->AddValueObserver(&dummy);
  controller_impl->AddValueObserver(&dummy_impl);

  AddOpacityTransitionToController(controller.get(), 1.0, 0.0f, 1.0f, false);
  controller->Animate(0.0);
  controller->UpdateState(true, NULL);
  controller->PushAnimationUpdatesTo(controller_impl.get());

  controller_impl->Animate(0.5);
  controller_impl->UpdateState(true, events.get());

  // There should be a Started event for the animation.
  EXPECT_EQ(1u, events->size());
  EXPECT_EQ(AnimationEvent::Started, (*events)[0].type);
  controller->NotifyAnimationStarted((*events)[0], 0.0);

  controller->Animate(1.0);
  controller->UpdateState(true, NULL);

  EXPECT_FALSE(dummy.animation_waiting_for_deletion());
  EXPECT_FALSE(dummy_impl.animation_waiting_for_deletion());

  events.reset(new AnimationEventsVector);
  controller_impl->Animate(2.0);
  controller_impl->UpdateState(true, events.get());

  EXPECT_TRUE(dummy_impl.animation_waiting_for_deletion());

  // There should be a Finished event for the animation.
  EXPECT_EQ(1u, events->size());
  EXPECT_EQ(AnimationEvent::Finished, (*events)[0].type);

  // Neither controller should have deleted the animation yet.
  EXPECT_TRUE(controller->GetAnimation(Animation::Opacity));
  EXPECT_TRUE(controller_impl->GetAnimation(Animation::Opacity));

  controller->NotifyAnimationFinished((*events)[0], 0.0);

  controller->Animate(3.0);
  controller->UpdateState(true, NULL);
  EXPECT_TRUE(dummy.animation_waiting_for_deletion());

  controller->PushAnimationUpdatesTo(controller_impl.get());

  // Both controllers should now have deleted the animation.
  EXPECT_FALSE(controller->has_any_animation());
  EXPECT_FALSE(controller_impl->has_any_animation());
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
  controller->AddValueObserver(&dummy);

  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));

  controller->AddAnimation(to_add.Pass());
  controller->Animate(0.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  // A non-impl-only animation should not generate property updates.
  const AnimationEvent* event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);
  controller->Animate(1.0);
  controller->UpdateState(true, events.get());
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
  controller_impl->AddValueObserver(&dummy_impl);

  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));
  to_add->set_is_impl_only(true);

  controller_impl->AddAnimation(to_add.Pass());
  controller_impl->Animate(0.0);
  controller_impl->UpdateState(true, events.get());
  EXPECT_TRUE(controller_impl->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy_impl.opacity());
  EXPECT_EQ(2u, events->size());
  const AnimationEvent* start_opacity_event =
      GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_EQ(0.f, start_opacity_event->opacity);

  controller_impl->Animate(1.0);
  controller_impl->UpdateState(true, events.get());
  EXPECT_EQ(1.f, dummy_impl.opacity());
  EXPECT_FALSE(controller_impl->HasActiveAnimation());
  EXPECT_EQ(4u, events->size());
  const AnimationEvent* end_opacity_event =
      GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_EQ(1.f, end_opacity_event->opacity);
}

TEST(LayerAnimationControllerTest, TrivialTransformOnImpl) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddValueObserver(&dummy_impl);

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
  controller_impl->UpdateState(true, events.get());
  EXPECT_TRUE(controller_impl->HasActiveAnimation());
  EXPECT_EQ(gfx::Transform(), dummy_impl.transform());
  EXPECT_EQ(2u, events->size());
  const AnimationEvent* start_transform_event =
      GetMostRecentPropertyUpdateEvent(events.get());
  ASSERT_TRUE(start_transform_event);
  EXPECT_EQ(gfx::Transform(), start_transform_event->transform);
  EXPECT_TRUE(start_transform_event->is_impl_only);

  gfx::Transform expected_transform;
  expected_transform.Translate(delta_x, delta_y);

  controller_impl->Animate(1.0);
  controller_impl->UpdateState(true, events.get());
  EXPECT_EQ(expected_transform, dummy_impl.transform());
  EXPECT_FALSE(controller_impl->HasActiveAnimation());
  EXPECT_EQ(4u, events->size());
  const AnimationEvent* end_transform_event =
      GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_EQ(expected_transform, end_transform_event->transform);
  EXPECT_TRUE(end_transform_event->is_impl_only);
}

TEST(LayerAnimationControllerTest, FilterTransition) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddValueObserver(&dummy);

  scoped_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());

  FilterOperations start_filters;
  start_filters.Append(FilterOperation::CreateBrightnessFilter(1.f));
  curve->AddKeyframe(FilterKeyframe::Create(
      0, start_filters, scoped_ptr<cc::TimingFunction>()));
  FilterOperations end_filters;
  end_filters.Append(FilterOperation::CreateBrightnessFilter(2.f));
  curve->AddKeyframe(FilterKeyframe::Create(
      1, end_filters, scoped_ptr<cc::TimingFunction>()));

  scoped_ptr<Animation> animation(Animation::Create(
      curve.PassAs<AnimationCurve>(), 1, 0, Animation::Filter));
  controller->AddAnimation(animation.Pass());

  controller->Animate(0.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(start_filters, dummy.filters());
  // A non-impl-only animation should not generate property updates.
  const AnimationEvent* event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  controller->Animate(0.5);
  controller->UpdateState(true, events.get());
  EXPECT_EQ(1u, dummy.filters().size());
  EXPECT_EQ(FilterOperation::CreateBrightnessFilter(1.5f),
            dummy.filters().at(0));
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);

  controller->Animate(1.0);
  controller->UpdateState(true, events.get());
  EXPECT_EQ(end_filters, dummy.filters());
  EXPECT_FALSE(controller->HasActiveAnimation());
  event = GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_FALSE(event);
}

TEST(LayerAnimationControllerTest, FilterTransitionOnImplOnly) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddValueObserver(&dummy_impl);

  scoped_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());

  // Create simple Filter animation.
  FilterOperations start_filters;
  start_filters.Append(FilterOperation::CreateBrightnessFilter(1.f));
  curve->AddKeyframe(FilterKeyframe::Create(
      0, start_filters, scoped_ptr<cc::TimingFunction>()));
  FilterOperations end_filters;
  end_filters.Append(FilterOperation::CreateBrightnessFilter(2.f));
  curve->AddKeyframe(FilterKeyframe::Create(
      1, end_filters, scoped_ptr<cc::TimingFunction>()));

  scoped_ptr<Animation> animation(Animation::Create(
      curve.PassAs<AnimationCurve>(), 1, 0, Animation::Filter));
  animation->set_is_impl_only(true);
  controller_impl->AddAnimation(animation.Pass());

  // Run animation.
  controller_impl->Animate(0.0);
  controller_impl->UpdateState(true, events.get());
  EXPECT_TRUE(controller_impl->HasActiveAnimation());
  EXPECT_EQ(start_filters, dummy_impl.filters());
  EXPECT_EQ(2u, events->size());
  const AnimationEvent* start_filter_event =
      GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_TRUE(start_filter_event);
  EXPECT_EQ(start_filters, start_filter_event->filters);
  EXPECT_TRUE(start_filter_event->is_impl_only);

  controller_impl->Animate(1.0);
  controller_impl->UpdateState(true, events.get());
  EXPECT_EQ(end_filters, dummy_impl.filters());
  EXPECT_FALSE(controller_impl->HasActiveAnimation());
  EXPECT_EQ(4u, events->size());
  const AnimationEvent* end_filter_event =
      GetMostRecentPropertyUpdateEvent(events.get());
  EXPECT_TRUE(end_filter_event);
  EXPECT_EQ(end_filters, end_filter_event->filters);
  EXPECT_TRUE(end_filter_event->is_impl_only);
}

class FakeAnimationDelegate : public AnimationDelegate {
 public:
  FakeAnimationDelegate()
      : started_(false),
        finished_(false) {}

  virtual void NotifyAnimationStarted(double time) OVERRIDE {
    started_ = true;
  }

  virtual void NotifyAnimationFinished(double time) OVERRIDE {
    finished_ = true;
  }

  bool started() { return started_; }

  bool finished() { return finished_; }

 private:
  bool started_;
  bool finished_;
};

// Tests that impl-only animations lead to start and finished notifications
// being sent to the main thread controller's animation delegate.
TEST(LayerAnimationControllerTest,
     NotificationsForImplOnlyAnimationsAreSentToMainThreadDelegate) {
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddValueObserver(&dummy_impl);
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddValueObserver(&dummy);
  FakeAnimationDelegate delegate;
  controller->set_layer_animation_delegate(&delegate);

  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));
  to_add->set_is_impl_only(true);
  controller_impl->AddAnimation(to_add.Pass());

  controller_impl->Animate(0.0);
  controller_impl->UpdateState(true, events.get());

  // We should receive 2 events (a started notification and a property update).
  EXPECT_EQ(2u, events->size());
  EXPECT_EQ(AnimationEvent::Started, (*events)[0].type);
  EXPECT_TRUE((*events)[0].is_impl_only);
  EXPECT_EQ(AnimationEvent::PropertyUpdate, (*events)[1].type);
  EXPECT_TRUE((*events)[1].is_impl_only);

  // Passing on the start event to the main thread controller should cause the
  // delegate to get notified.
  EXPECT_FALSE(delegate.started());
  controller->NotifyAnimationStarted((*events)[0], 0.0);
  EXPECT_TRUE(delegate.started());

  events.reset(new AnimationEventsVector);
  controller_impl->Animate(1.0);
  controller_impl->UpdateState(true, events.get());

  // We should receive 2 events (a finished notification and a property update).
  EXPECT_EQ(2u, events->size());
  EXPECT_EQ(AnimationEvent::Finished, (*events)[0].type);
  EXPECT_TRUE((*events)[0].is_impl_only);
  EXPECT_EQ(AnimationEvent::PropertyUpdate, (*events)[1].type);
  EXPECT_TRUE((*events)[1].is_impl_only);

  // Passing on the finished event to the main thread controller should cause
  // the delegate to get notified.
  EXPECT_FALSE(delegate.finished());
  controller->NotifyAnimationFinished((*events)[0], 0.0);
  EXPECT_TRUE(delegate.finished());
}

// Tests animations that are waiting for a synchronized start time do not
// finish.
TEST(LayerAnimationControllerTest,
     AnimationsWaitingForStartTimeDoNotFinishIfTheyOutwaitTheirFinish) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddValueObserver(&dummy);

  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));
  to_add->set_needs_synchronized_start_time(true);

  // We should pause at the first keyframe indefinitely waiting for that
  // animation to start.
  controller->AddAnimation(to_add.Pass());
  controller->Animate(0.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(1.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(2.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());

  // Send the synchronized start time.
  controller->NotifyAnimationStarted(
      AnimationEvent(AnimationEvent::Started, 0, 1, Animation::Opacity, 2),
      0.0);
  controller->Animate(5.0);
  controller->UpdateState(true, events.get());
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
  controller->AddValueObserver(&dummy);

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
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(1.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(1.f, dummy.opacity());
  controller->Animate(2.0);
  controller->UpdateState(true, events.get());
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
  controller->AddValueObserver(&dummy);
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));
  controller->Animate(0.0);
  controller->UpdateState(true, events.get());
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
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(1.f, dummy.opacity());
  controller->Animate(1.5);
  controller->UpdateState(true, events.get());
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
  controller->AddValueObserver(&dummy);

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
  controller->UpdateState(true, events.get());
  EXPECT_EQ(0.f, dummy.opacity());
  EXPECT_TRUE(controller->HasActiveAnimation());
  controller->Animate(1.0);
  controller->UpdateState(true, events.get());
  // Should not have started the float transition yet.
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  // The float animation should have started at time 1 and should be done.
  controller->Animate(2.0);
  controller->UpdateState(true, events.get());
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
  controller->AddValueObserver(&dummy);

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
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  // The opacity animation should have finished at time 1, but the group
  // of animations with id 1 don't finish until time 2 because of the length
  // of the transform animation.
  controller->Animate(2.0);
  controller->UpdateState(true, events.get());
  // Should not have started the float transition yet.
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(1.f, dummy.opacity());

  // The second opacity animation should start at time 2 and should be done by
  // time 3.
  controller->Animate(3.0);
  controller->UpdateState(true, events.get());
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
  controller->AddValueObserver(&dummy);

  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));
  to_add->SetRunState(Animation::WaitingForStartTime, 0);
  to_add->set_start_time(1.f);
  controller->AddAnimation(to_add.Pass());

  controller->Animate(0.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(1.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(2.0);
  controller->UpdateState(true, events.get());
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
  controller->AddValueObserver(&dummy);

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
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(0.5);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.25f, dummy.opacity());
  controller->Animate(1.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.5f, dummy.opacity());
  controller->Animate(2.0);
  controller->UpdateState(true, events.get());
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
  controller->AddValueObserver(&dummy);

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
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(0.5);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.25f, dummy.opacity());
  EXPECT_TRUE(controller->HasActiveAnimation());
  controller->Animate(1.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.5f, dummy.opacity());
  controller->Animate(3.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(4.0);
  controller->UpdateState(true, events.get());
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
  controller->AddValueObserver(&dummy);

  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));
  to_add->set_iterations(3);
  controller->AddAnimation(to_add.Pass());

  controller->Animate(0.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(1.25);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.25f, dummy.opacity());
  controller->Animate(1.75);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.75f, dummy.opacity());
  controller->Animate(2.25);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.25f, dummy.opacity());
  controller->Animate(2.75);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.75f, dummy.opacity());
  controller->Animate(3.0);
  controller->UpdateState(true, events.get());
  EXPECT_FALSE(controller->HasActiveAnimation());
  EXPECT_EQ(1.f, dummy.opacity());

  // Just be extra sure.
  controller->Animate(4.0);
  controller->UpdateState(true, events.get());
  EXPECT_EQ(1.f, dummy.opacity());
}

// Test that an infinitely looping animation does indeed go until aborted.
TEST(LayerAnimationControllerTest, InfiniteLooping) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddValueObserver(&dummy);

  const int id = 1;
  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      id,
      Animation::Opacity));
  to_add->set_iterations(-1);
  controller->AddAnimation(to_add.Pass());

  controller->Animate(0.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(1.25);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.25f, dummy.opacity());
  controller->Animate(1.75);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.75f, dummy.opacity());

  controller->Animate(1073741824.25);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.25f, dummy.opacity());
  controller->Animate(1073741824.75);
  controller->UpdateState(true, events.get());
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
  controller->AddValueObserver(&dummy);

  const int id = 1;
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      id,
      Animation::Opacity));

  controller->Animate(0.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(0.5);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.5f, dummy.opacity());

  EXPECT_TRUE(controller->GetAnimation(id, Animation::Opacity));
  controller->GetAnimation(id, Animation::Opacity)->SetRunState(
      Animation::Paused, 0.5);

  controller->Animate(1024);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.5f, dummy.opacity());

  EXPECT_TRUE(controller->GetAnimation(id, Animation::Opacity));
  controller->GetAnimation(id, Animation::Opacity)->SetRunState(
      Animation::Running, 1024);

  controller->Animate(1024.25);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.75f, dummy.opacity());
  controller->Animate(1024.5);
  controller->UpdateState(true, events.get());
  EXPECT_FALSE(controller->HasActiveAnimation());
  EXPECT_EQ(1.f, dummy.opacity());
}

TEST(LayerAnimationControllerTest, AbortAGroupedAnimation) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddValueObserver(&dummy);

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
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());
  controller->Animate(1.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.5f, dummy.opacity());

  EXPECT_TRUE(controller->GetAnimation(id, Animation::Opacity));
  controller->GetAnimation(id, Animation::Opacity)->SetRunState(
      Animation::Aborted, 1);
  controller->Animate(1.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(1.f, dummy.opacity());
  controller->Animate(2.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(!controller->HasActiveAnimation());
  EXPECT_EQ(0.75f, dummy.opacity());
}

TEST(LayerAnimationControllerTest, PushUpdatesWhenSynchronizedStartTimeNeeded) {
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddValueObserver(&dummy_impl);
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddValueObserver(&dummy);

  scoped_ptr<Animation> to_add(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(2.0, 0.f, 1.f)).Pass(),
      0,
      Animation::Opacity));
  to_add->set_needs_synchronized_start_time(true);
  controller->AddAnimation(to_add.Pass());

  controller->Animate(0.0);
  controller->UpdateState(true, events.get());
  EXPECT_TRUE(controller->HasActiveAnimation());
  Animation* active_animation = controller->GetAnimation(0, Animation::Opacity);
  EXPECT_TRUE(active_animation);
  EXPECT_TRUE(active_animation->needs_synchronized_start_time());

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
  controller->AddValueObserver(&dummy);

  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeTransformTransition(1)).Pass(),
      1,
      Animation::Transform));

  controller->Animate(0.0);
  controller->UpdateState(true, events.get());

  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      2,
      Animation::Opacity));

  // Animate but don't UpdateState.
  controller->Animate(1.0);

  controller->Animate(2.0);
  events.reset(new AnimationEventsVector);
  controller->UpdateState(true, events.get());

  // Should have one Started event and one Finished event.
  EXPECT_EQ(2u, events->size());
  EXPECT_NE((*events)[0].type, (*events)[1].type);

  // The float transition should still be at its starting point.
  EXPECT_TRUE(controller->HasActiveAnimation());
  EXPECT_EQ(0.f, dummy.opacity());

  controller->Animate(3.0);
  controller->UpdateState(true, events.get());

  // The float tranisition should now be done.
  EXPECT_EQ(1.f, dummy.opacity());
  EXPECT_FALSE(controller->HasActiveAnimation());
}

// Tests that an animation controller with only an inactive observer gets ticked
// but doesn't progress animations past the Starting state.
TEST(LayerAnimationControllerTest, InactiveObserverGetsTicked) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy;
  FakeInactiveLayerAnimationValueObserver inactive_dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));

  const int id = 1;
  controller->AddAnimation(CreateAnimation(scoped_ptr<AnimationCurve>(
      new FakeFloatTransition(1.0, 0.5f, 1.f)).Pass(),
      id,
      Animation::Opacity));

  // Without an observer, the animation shouldn't progress to the Starting
  // state.
  controller->Animate(0.0);
  controller->UpdateState(true, events.get());
  EXPECT_EQ(0u, events->size());
  EXPECT_EQ(Animation::WaitingForTargetAvailability,
            controller->GetAnimation(id, Animation::Opacity)->run_state());

  controller->AddValueObserver(&inactive_dummy);

  // With only an inactive observer, the animation should progress to the
  // Starting state and get ticked at its starting point, but should not
  // progress to Running.
  controller->Animate(1.0);
  controller->UpdateState(true, events.get());
  EXPECT_EQ(0u, events->size());
  EXPECT_EQ(Animation::Starting,
            controller->GetAnimation(id, Animation::Opacity)->run_state());
  EXPECT_EQ(0.5f, inactive_dummy.opacity());

  // Even when already in the Starting state, the animation should stay
  // there, and shouldn't be ticked past its starting point.
  controller->Animate(2.0);
  controller->UpdateState(true, events.get());
  EXPECT_EQ(0u, events->size());
  EXPECT_EQ(Animation::Starting,
            controller->GetAnimation(id, Animation::Opacity)->run_state());
  EXPECT_EQ(0.5f, inactive_dummy.opacity());

  controller->AddValueObserver(&dummy);

  // Now that an active observer has been added, the animation should still
  // initially tick at its starting point, but should now progress to Running.
  controller->Animate(3.0);
  controller->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->size());
  EXPECT_EQ(Animation::Running,
            controller->GetAnimation(id, Animation::Opacity)->run_state());
  EXPECT_EQ(0.5f, inactive_dummy.opacity());
  EXPECT_EQ(0.5f, dummy.opacity());

  // The animation should now tick past its starting point.
  controller->Animate(3.5);
  EXPECT_NE(0.5f, inactive_dummy.opacity());
  EXPECT_NE(0.5f, dummy.opacity());
}

TEST(LayerAnimationControllerTest, AnimatedBounds) {
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));

  scoped_ptr<KeyframedTransformAnimationCurve> curve1(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1;
  curve1->AddKeyframe(TransformKeyframe::Create(
      0.0, operations1, scoped_ptr<TimingFunction>()));
  operations1.AppendTranslate(10.0, 15.0, 0.0);
  curve1->AddKeyframe(TransformKeyframe::Create(
      1.0, operations1, scoped_ptr<TimingFunction>()));

  scoped_ptr<Animation> animation(Animation::Create(
      curve1.PassAs<AnimationCurve>(), 1, 1, Animation::Transform));
  controller_impl->AddAnimation(animation.Pass());

  scoped_ptr<KeyframedTransformAnimationCurve> curve2(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations2;
  curve2->AddKeyframe(TransformKeyframe::Create(
      0.0, operations2, scoped_ptr<TimingFunction>()));
  operations2.AppendScale(2.0, 3.0, 4.0);
  curve2->AddKeyframe(TransformKeyframe::Create(
      1.0, operations2, scoped_ptr<TimingFunction>()));

  animation = Animation::Create(
      curve2.PassAs<AnimationCurve>(), 2, 2, Animation::Transform);
  controller_impl->AddAnimation(animation.Pass());

  gfx::BoxF box(1.f, 2.f, -1.f, 3.f, 4.f, 5.f);
  gfx::BoxF bounds;

  EXPECT_TRUE(controller_impl->AnimatedBoundsForBox(box, &bounds));
  EXPECT_EQ(gfx::BoxF(1.f, 2.f, -4.f, 13.f, 19.f, 20.f).ToString(),
            bounds.ToString());

  controller_impl->GetAnimation(1, Animation::Transform)->SetRunState(
      cc::Animation::Finished, 0.0);

  // Only the unfinished animation should affect the animated bounds.
  EXPECT_TRUE(controller_impl->AnimatedBoundsForBox(box, &bounds));
  EXPECT_EQ(gfx::BoxF(1.f, 2.f, -4.f, 7.f, 16.f, 20.f).ToString(),
            bounds.ToString());

  controller_impl->GetAnimation(2, Animation::Transform)->SetRunState(
      cc::Animation::Finished, 0.0);

  // There are no longer any running animations.
  EXPECT_TRUE(controller_impl->AnimatedBoundsForBox(box, &bounds));
  EXPECT_EQ(gfx::BoxF().ToString(), bounds.ToString());

  // Add an animation whose bounds we don't yet support computing.
  scoped_ptr<KeyframedTransformAnimationCurve> curve3(
      KeyframedTransformAnimationCurve::Create());
  TransformOperations operations3;
  gfx::Transform transform3;
  transform3.Scale3d(1.0, 2.0, 3.0);
  curve3->AddKeyframe(TransformKeyframe::Create(
      0.0, operations3, scoped_ptr<TimingFunction>()));
  operations3.AppendMatrix(transform3);
  curve3->AddKeyframe(TransformKeyframe::Create(
      1.0, operations3, scoped_ptr<TimingFunction>()));
  animation = Animation::Create(
      curve3.PassAs<AnimationCurve>(), 3, 3, Animation::Transform);
  controller_impl->AddAnimation(animation.Pass());
  EXPECT_FALSE(controller_impl->AnimatedBoundsForBox(box, &bounds));
}

// Tests that AbortAnimations aborts all animations targeting the specified
// property.
TEST(LayerAnimationControllerTest, AbortAnimations) {
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddValueObserver(&dummy);

  // Start with several animations, and allow some of them to reach the finished
  // state.
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeTransformTransition(1.0)).Pass(),
      1,
      Animation::Transform));
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      2,
      Animation::Opacity));
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeTransformTransition(1.0)).Pass(),
      3,
      Animation::Transform));
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeTransformTransition(2.0)).Pass(),
      4,
      Animation::Transform));
  controller->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      5,
      Animation::Opacity));

  controller->Animate(1.0);
  controller->UpdateState(true, NULL);
  controller->Animate(2.0);
  controller->UpdateState(true, NULL);

  EXPECT_EQ(Animation::Finished,
            controller->GetAnimation(1, Animation::Transform)->run_state());
  EXPECT_EQ(Animation::Finished,
            controller->GetAnimation(2, Animation::Opacity)->run_state());
  EXPECT_EQ(Animation::Running,
            controller->GetAnimation(3, Animation::Transform)->run_state());
  EXPECT_EQ(Animation::WaitingForTargetAvailability,
            controller->GetAnimation(4, Animation::Transform)->run_state());
  EXPECT_EQ(Animation::Running,
            controller->GetAnimation(5, Animation::Opacity)->run_state());

  controller->AbortAnimations(Animation::Transform);

  // Only un-finished Transform animations should have been aborted.
  EXPECT_EQ(Animation::Finished,
            controller->GetAnimation(1, Animation::Transform)->run_state());
  EXPECT_EQ(Animation::Finished,
            controller->GetAnimation(2, Animation::Opacity)->run_state());
  EXPECT_EQ(Animation::Aborted,
            controller->GetAnimation(3, Animation::Transform)->run_state());
  EXPECT_EQ(Animation::Aborted,
            controller->GetAnimation(4, Animation::Transform)->run_state());
  EXPECT_EQ(Animation::Running,
            controller->GetAnimation(5, Animation::Opacity)->run_state());
}

// An animation aborted on the main thread should get deleted on both threads.
TEST(LayerAnimationControllerTest, MainThreadAbortedAnimationGetsDeleted) {
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddValueObserver(&dummy_impl);
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddValueObserver(&dummy);

  AddOpacityTransitionToController(controller.get(), 1.0, 0.f, 1.f, false);
  int group_id = controller->GetAnimation(Animation::Opacity)->group();

  controller->PushAnimationUpdatesTo(controller_impl.get());
  EXPECT_TRUE(controller_impl->GetAnimation(group_id, Animation::Opacity));

  controller->AbortAnimations(Animation::Opacity);
  EXPECT_EQ(Animation::Aborted,
            controller->GetAnimation(Animation::Opacity)->run_state());
  EXPECT_FALSE(dummy.animation_waiting_for_deletion());
  EXPECT_FALSE(dummy_impl.animation_waiting_for_deletion());

  controller->Animate(1.0);
  controller->UpdateState(true, NULL);
  EXPECT_TRUE(dummy.animation_waiting_for_deletion());
  EXPECT_EQ(Animation::WaitingForDeletion,
            controller->GetAnimation(Animation::Opacity)->run_state());

  controller->PushAnimationUpdatesTo(controller_impl.get());
  EXPECT_FALSE(controller->GetAnimation(group_id, Animation::Opacity));
  EXPECT_FALSE(controller_impl->GetAnimation(group_id, Animation::Opacity));
}

// An animation aborted on the impl thread should get deleted on both threads.
TEST(LayerAnimationControllerTest, ImplThreadAbortedAnimationGetsDeleted) {
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddValueObserver(&dummy_impl);
  FakeLayerAnimationValueObserver dummy;
  scoped_refptr<LayerAnimationController> controller(
      LayerAnimationController::Create(0));
  controller->AddValueObserver(&dummy);

  AddOpacityTransitionToController(controller.get(), 1.0, 0.f, 1.f, false);
  int group_id = controller->GetAnimation(Animation::Opacity)->group();

  controller->PushAnimationUpdatesTo(controller_impl.get());
  EXPECT_TRUE(controller_impl->GetAnimation(group_id, Animation::Opacity));

  controller_impl->AbortAnimations(Animation::Opacity);
  EXPECT_EQ(Animation::Aborted,
            controller_impl->GetAnimation(Animation::Opacity)->run_state());
  EXPECT_FALSE(dummy.animation_waiting_for_deletion());
  EXPECT_FALSE(dummy_impl.animation_waiting_for_deletion());

  AnimationEventsVector events;
  controller_impl->Animate(1.0);
  controller_impl->UpdateState(true, &events);
  EXPECT_TRUE(dummy_impl.animation_waiting_for_deletion());
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(AnimationEvent::Aborted, events[0].type);
  EXPECT_EQ(Animation::WaitingForDeletion,
            controller_impl->GetAnimation(Animation::Opacity)->run_state());

  controller->NotifyAnimationAborted(events[0]);
  EXPECT_EQ(Animation::Aborted,
            controller->GetAnimation(Animation::Opacity)->run_state());

  controller->Animate(1.5);
  controller->UpdateState(true, NULL);
  EXPECT_TRUE(dummy.animation_waiting_for_deletion());
  EXPECT_EQ(Animation::WaitingForDeletion,
            controller->GetAnimation(Animation::Opacity)->run_state());

  controller->PushAnimationUpdatesTo(controller_impl.get());
  EXPECT_FALSE(controller->GetAnimation(group_id, Animation::Opacity));
  EXPECT_FALSE(controller_impl->GetAnimation(group_id, Animation::Opacity));
}

// Ensure that we only generate Finished events for animations in a group
// once all animations in that group are finished.
TEST(LayerAnimationControllerTest, FinishedEventsForGroup) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddValueObserver(&dummy_impl);

  // Add two animations with the same group id but different durations.
  controller_impl->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeTransformTransition(2.0)).Pass(),
      1,
      Animation::Transform));
  controller_impl->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));

  controller_impl->Animate(1.0);
  controller_impl->UpdateState(true, events.get());

  // Both animations should have started.
  EXPECT_EQ(2u, events->size());
  EXPECT_EQ(AnimationEvent::Started, (*events)[0].type);
  EXPECT_EQ(AnimationEvent::Started, (*events)[1].type);

  events.reset(new AnimationEventsVector);
  controller_impl->Animate(2.0);
  controller_impl->UpdateState(true, events.get());

  // The opacity animation should be finished, but should not have generated
  // a Finished event yet.
  EXPECT_EQ(0u, events->size());
  EXPECT_EQ(Animation::Finished,
            controller_impl->GetAnimation(1, Animation::Opacity)->run_state());
  EXPECT_EQ(Animation::Running,
            controller_impl->GetAnimation(1,
                                          Animation::Transform)->run_state());

  controller_impl->Animate(3.0);
  controller_impl->UpdateState(true, events.get());

  // Both animations should have generated Finished events.
  EXPECT_EQ(2u, events->size());
  EXPECT_EQ(AnimationEvent::Finished, (*events)[0].type);
  EXPECT_EQ(AnimationEvent::Finished, (*events)[1].type);
}

// Ensure that when a group has a mix of aborted and finished animations,
// we generate a Finished event for the finished animation and an Aborted
// event for the aborted animation.
TEST(LayerAnimationControllerTest, FinishedAndAbortedEventsForGroup) {
  scoped_ptr<AnimationEventsVector> events(
      make_scoped_ptr(new AnimationEventsVector));
  FakeLayerAnimationValueObserver dummy_impl;
  scoped_refptr<LayerAnimationController> controller_impl(
      LayerAnimationController::Create(0));
  controller_impl->AddValueObserver(&dummy_impl);

  // Add two animations with the same group id.
  controller_impl->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeTransformTransition(1.0)).Pass(),
      1,
      Animation::Transform));
  controller_impl->AddAnimation(CreateAnimation(
      scoped_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)).Pass(),
      1,
      Animation::Opacity));

  controller_impl->Animate(1.0);
  controller_impl->UpdateState(true, events.get());

  // Both animations should have started.
  EXPECT_EQ(2u, events->size());
  EXPECT_EQ(AnimationEvent::Started, (*events)[0].type);
  EXPECT_EQ(AnimationEvent::Started, (*events)[1].type);

  controller_impl->AbortAnimations(Animation::Opacity);

  events.reset(new AnimationEventsVector);
  controller_impl->Animate(2.0);
  controller_impl->UpdateState(true, events.get());

  // We should have exactly 2 events: a Finished event for the tranform
  // animation, and an Aborted event for the opacity animation.
  EXPECT_EQ(2u, events->size());
  EXPECT_EQ(AnimationEvent::Finished, (*events)[0].type);
  EXPECT_EQ(Animation::Transform, (*events)[0].target_property);
  EXPECT_EQ(AnimationEvent::Aborted, (*events)[1].type);
  EXPECT_EQ(Animation::Opacity, (*events)[1].target_property);
}

}  // namespace
}  // namespace cc
