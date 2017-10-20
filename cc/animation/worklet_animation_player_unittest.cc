// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "cc/animation/worklet_animation_player.h"

#include "cc/test/animation_test_common.h"
#include "cc/test/animation_timelines_test_common.h"
#include "cc/test/mock_layer_tree_mutator.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Mock;
using ::testing::Return;
using ::testing::_;

namespace cc {

namespace {

class WorkletAnimationPlayerTest : public AnimationTimelinesTest {
 public:
  WorkletAnimationPlayerTest() {}
  ~WorkletAnimationPlayerTest() override {}

  int worklet_player_id_ = 11;
};

TEST_F(WorkletAnimationPlayerTest, LocalTimeIsUsedWithAnimations) {
  client_.RegisterElement(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElement(element_id_, ElementListType::PENDING);
  client_impl_.RegisterElement(element_id_, ElementListType::ACTIVE);

  const float start_opacity = .7f;
  const float end_opacity = .3f;
  const double duration = 1.;

  const float expected_opacity =
      start_opacity + (end_opacity - start_opacity) / 2;

  scoped_refptr<WorkletAnimationPlayer> worklet_player_ =
      WorkletAnimationPlayer::Create(worklet_player_id_, "test_name");

  worklet_player_->AttachElement(element_id_);
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(worklet_player_);

  AddOpacityTransitionToPlayer(worklet_player_.get(), duration, start_opacity,
                               end_opacity, true);

  host_->PushPropertiesTo(host_impl_);
  host_impl_->ActivateAnimations();

  // TODO(majidvp): At the moment the player does not use the local time when
  // it is starting. This is because Animation::ConvertToActiveTime always
  // returns the time_offset when starting. We need to change this.
  base::TimeTicks time;
  time += base::TimeDelta::FromSecondsD(0.1);
  TickAnimationsTransferEvents(time, 1u);

  base::TimeDelta local_time = base::TimeDelta::FromSecondsD(duration / 2);
  worklet_player_->SetLocalTime(local_time);

  host_->PushPropertiesTo(host_impl_);

  TickAnimationsTransferEvents(time, 0u);

  client_.ExpectOpacityPropertyMutated(element_id_, ElementListType::ACTIVE,
                                       expected_opacity);

  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::ACTIVE, expected_opacity);
}

// Tests that verify interaction of AnimationHost with LayerTreeMutator.
// TODO(majidvp): Perhaps moves these to AnimationHostTest.
TEST_F(WorkletAnimationPlayerTest,
       LayerTreeMutatorsIsMutatedWithCorrectInputState) {
  MockLayerTreeMutator* mock_mutator = new MockLayerTreeMutator();
  host_impl_->SetLayerTreeMutator(
      base::WrapUnique<LayerTreeMutator>(mock_mutator));
  ON_CALL(*mock_mutator, HasAnimators()).WillByDefault(Return(true));

  client_.RegisterElement(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElement(element_id_, ElementListType::PENDING);
  client_impl_.RegisterElement(element_id_, ElementListType::ACTIVE);

  const float start_opacity = .7f;
  const float end_opacity = .3f;
  const double duration = 1.;

  scoped_refptr<WorkletAnimationPlayer> worklet_player_ =
      WorkletAnimationPlayer::Create(worklet_player_id_, "test_name");

  worklet_player_->AttachElement(element_id_);
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(worklet_player_);

  AddOpacityTransitionToPlayer(worklet_player_.get(), duration, start_opacity,
                               end_opacity, true);

  host_->PushPropertiesTo(host_impl_);
  host_impl_->ActivateAnimations();

  EXPECT_CALL(*mock_mutator, MutateRef(_, _));

  base::TimeTicks time;
  time += base::TimeDelta::FromSecondsD(0.1);
  TickAnimationsTransferEvents(time, 1u);

  Mock::VerifyAndClearExpectations(mock_mutator);
}

}  // namespace

}  // namespace cc
