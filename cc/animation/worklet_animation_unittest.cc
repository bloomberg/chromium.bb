// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/worklet_animation.h"

#include "base/memory/ptr_util.h"
#include "cc/animation/scroll_timeline.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/animation_timelines_test_common.h"
#include "cc/test/mock_layer_tree_mutator.h"
#include "cc/trees/property_tree.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

namespace cc {

namespace {

class WorkletAnimationTest : public AnimationTimelinesTest {
 public:
  WorkletAnimationTest() = default;
  ~WorkletAnimationTest() override = default;

  void AttachWorkletAnimation() {
    client_.RegisterElement(element_id_, ElementListType::ACTIVE);
    client_impl_.RegisterElement(element_id_, ElementListType::PENDING);
    client_impl_.RegisterElement(element_id_, ElementListType::ACTIVE);

    worklet_animation_ = WorkletAnimation::Create(
        worklet_animation_id_, "test_name", nullptr, nullptr);

    worklet_animation_->AttachElement(element_id_);
    host_->AddAnimationTimeline(timeline_);
    timeline_->AttachAnimation(worklet_animation_);

    host_->PushPropertiesTo(host_impl_);
    timeline_impl_ = host_impl_->GetTimelineById(timeline_id_);
    worklet_animation_impl_ = static_cast<WorkletAnimation*>(
        timeline_impl_->GetAnimationById(worklet_animation_id_));
  }

  scoped_refptr<WorkletAnimation> worklet_animation_;
  scoped_refptr<WorkletAnimation> worklet_animation_impl_;
  int worklet_animation_id_ = 11;
};

class MockScrollTimeline : public ScrollTimeline {
 public:
  MockScrollTimeline()
      : ScrollTimeline(ElementId(), ScrollTimeline::Vertical, 0) {}
  MOCK_CONST_METHOD1(CurrentTime, double(const ScrollTree&));
};

TEST_F(WorkletAnimationTest, LocalTimeIsUsedWithAnimations) {
  AttachWorkletAnimation();

  const float start_opacity = .7f;
  const float end_opacity = .3f;
  const double duration = 1.;

  const float expected_opacity =
      start_opacity + (end_opacity - start_opacity) / 2;
  AddOpacityTransitionToAnimation(worklet_animation_.get(), duration,
                                  start_opacity, end_opacity, true);

  // Push the opacity animation to the impl thread.
  host_->PushPropertiesTo(host_impl_);
  host_impl_->ActivateAnimations();

  base::TimeDelta local_time = base::TimeDelta::FromSecondsD(duration / 2);

  worklet_animation_impl_->SetOutputState({0, local_time});

  TickAnimationsTransferEvents(base::TimeTicks(), 0u);

  TestLayer* layer =
      client_.FindTestLayer(element_id_, ElementListType::ACTIVE);
  EXPECT_FALSE(layer->is_property_mutated(TargetProperty::OPACITY));
  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::ACTIVE, expected_opacity);
}

// Tests that verify interaction of AnimationHost with LayerTreeMutator.
// TODO(majidvp): Perhaps moves these to AnimationHostTest.
TEST_F(WorkletAnimationTest, LayerTreeMutatorsIsMutatedWithCorrectInputState) {
  AttachWorkletAnimation();

  MockLayerTreeMutator* mock_mutator = new NiceMock<MockLayerTreeMutator>();
  host_impl_->SetLayerTreeMutator(
      base::WrapUnique<LayerTreeMutator>(mock_mutator));
  ON_CALL(*mock_mutator, HasAnimators()).WillByDefault(Return(true));

  const float start_opacity = .7f;
  const float end_opacity = .3f;
  const double duration = 1.;

  AddOpacityTransitionToAnimation(worklet_animation_.get(), duration,
                                  start_opacity, end_opacity, true);

  host_->PushPropertiesTo(host_impl_);
  host_impl_->ActivateAnimations();

  EXPECT_CALL(*mock_mutator, MutateRef(_));

  base::TimeTicks time;
  time += base::TimeDelta::FromSecondsD(0.1);
  TickAnimationsTransferEvents(time, 0u);

  Mock::VerifyAndClearExpectations(mock_mutator);
}

TEST_F(WorkletAnimationTest, LayerTreeMutatorsIsMutatedOnlyWhenInputChanges) {
  AttachWorkletAnimation();

  MockLayerTreeMutator* mock_mutator = new NiceMock<MockLayerTreeMutator>();
  host_impl_->SetLayerTreeMutator(
      base::WrapUnique<LayerTreeMutator>(mock_mutator));
  ON_CALL(*mock_mutator, HasAnimators()).WillByDefault(Return(true));

  const float start_opacity = .7f;
  const float end_opacity = .3f;
  const double duration = 1.;

  AddOpacityTransitionToAnimation(worklet_animation_.get(), duration,
                                  start_opacity, end_opacity, true);

  host_->PushPropertiesTo(host_impl_);
  host_impl_->ActivateAnimations();

  EXPECT_CALL(*mock_mutator, MutateRef(_)).Times(1);

  base::TimeTicks time;
  time += base::TimeDelta::FromSecondsD(0.1);
  TickAnimationsTransferEvents(time, 0u);

  // The time has not changed which means worklet animation input is the same.
  // Ticking animations again should not result in mutator being asked to
  // mutate.
  TickAnimationsTransferEvents(time, 0u);

  Mock::VerifyAndClearExpectations(mock_mutator);
}

TEST_F(WorkletAnimationTest, CurrentTimeCorrectlyUsesScrollTimeline) {
  auto scroll_timeline = std::make_unique<MockScrollTimeline>();
  EXPECT_CALL(*scroll_timeline, CurrentTime(_)).WillOnce(Return(1234));
  scoped_refptr<WorkletAnimation> worklet_animation = WorkletAnimation::Create(
      worklet_animation_id_, "test_name", std::move(scroll_timeline), nullptr);

  ScrollTree scroll_tree;
  MutatorInputState::AnimationState state =
      worklet_animation->GetInputState(base::TimeTicks::Now(), scroll_tree);
  EXPECT_EQ(1234, state.current_time);
}

TEST_F(WorkletAnimationTest,
       CurrentTimeFromDocumentTimelineIsOffsetByStartTime) {
  scoped_refptr<WorkletAnimation> worklet_animation = WorkletAnimation::Create(
      worklet_animation_id_, "test_name", nullptr, nullptr);

  base::TimeTicks first_ticks =
      base::TimeTicks() + base::TimeDelta::FromMillisecondsD(111);
  base::TimeTicks second_ticks =
      base::TimeTicks() + base::TimeDelta::FromMillisecondsD(111 + 123.4);
  base::TimeTicks third_ticks =
      base::TimeTicks() + base::TimeDelta::FromMillisecondsD(111 + 246.8);

  ScrollTree scroll_tree;
  MutatorInputState::AnimationState first_state =
      worklet_animation->GetInputState(first_ticks, scroll_tree);
  // First state request sets the start time and thus current time should be 0.
  EXPECT_EQ(0, first_state.current_time);
  MutatorInputState::AnimationState second_state =
      worklet_animation->GetInputState(second_ticks, scroll_tree);
  EXPECT_EQ(123.4, second_state.current_time);
  // Should always offset from start time.
  MutatorInputState::AnimationState third_state =
      worklet_animation->GetInputState(third_ticks, scroll_tree);
  EXPECT_EQ(246.8, third_state.current_time);
}

TEST_F(WorkletAnimationTest, NeedsUpdateCorrectlyReflectsInputTimeChange) {
  scoped_refptr<WorkletAnimation> worklet_animation = WorkletAnimation::Create(
      worklet_animation_id_, "test_name", nullptr, nullptr);

  base::TimeTicks first_ticks =
      base::TimeTicks() + base::TimeDelta::FromMillisecondsD(111);
  base::TimeTicks second_ticks =
      base::TimeTicks() + base::TimeDelta::FromMillisecondsD(111 + 123.4);

  ScrollTree scroll_tree;
  // First time should always be true.
  EXPECT_TRUE(worklet_animation->NeedsUpdate(first_ticks, scroll_tree));
  worklet_animation->GetInputState(first_ticks, scroll_tree);
  // Should be false if time is not different from last GetState.
  EXPECT_FALSE(worklet_animation->NeedsUpdate(first_ticks, scroll_tree));
  // Should be true when input time is different.
  EXPECT_TRUE(worklet_animation->NeedsUpdate(second_ticks, scroll_tree));
  // Should be side-effect free.
  EXPECT_TRUE(worklet_animation->NeedsUpdate(second_ticks, scroll_tree));
}

}  // namespace

}  // namespace cc
