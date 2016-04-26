// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_player.h"

#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/element_animations.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/animation_timelines_test_common.h"

namespace cc {
namespace {

class AnimationPlayerTest : public AnimationTimelinesTest {
 public:
  AnimationPlayerTest() {}
  ~AnimationPlayerTest() override {}
};

// See element_animations_unittest.cc for active/pending observers tests.

TEST_F(AnimationPlayerTest, AttachDetachLayerIfTimelineAttached) {
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(player_);
  EXPECT_FALSE(player_->element_animations());
  EXPECT_FALSE(player_->element_id());

  host_->PushPropertiesTo(host_impl_);

  EXPECT_FALSE(GetImplPlayerForLayerId(element_id_));

  GetImplTimelineAndPlayerByID();

  EXPECT_FALSE(player_impl_->element_animations());
  EXPECT_FALSE(player_impl_->element_id());

  player_->AttachLayer(element_id_);
  EXPECT_EQ(player_, GetPlayerForLayerId(element_id_));
  EXPECT_TRUE(player_->element_animations());
  EXPECT_EQ(player_->element_id(), element_id_);

  host_->PushPropertiesTo(host_impl_);

  EXPECT_EQ(player_impl_, GetImplPlayerForLayerId(element_id_));
  EXPECT_TRUE(player_impl_->element_animations());
  EXPECT_EQ(player_impl_->element_id(), element_id_);

  player_->DetachLayer();
  EXPECT_FALSE(GetPlayerForLayerId(element_id_));
  EXPECT_FALSE(player_->element_animations());
  EXPECT_FALSE(player_->element_id());

  host_->PushPropertiesTo(host_impl_);

  EXPECT_FALSE(GetImplPlayerForLayerId(element_id_));
  EXPECT_FALSE(player_impl_->element_animations());
  EXPECT_FALSE(player_impl_->element_id());

  timeline_->DetachPlayer(player_);
  EXPECT_FALSE(player_->animation_timeline());
  EXPECT_FALSE(player_->element_animations());
  EXPECT_FALSE(player_->element_id());
}

TEST_F(AnimationPlayerTest, AttachDetachTimelineIfLayerAttached) {
  host_->AddAnimationTimeline(timeline_);

  EXPECT_FALSE(player_->element_animations());
  EXPECT_FALSE(player_->element_id());

  player_->AttachLayer(element_id_);
  EXPECT_FALSE(player_->animation_timeline());
  EXPECT_FALSE(GetPlayerForLayerId(element_id_));
  EXPECT_FALSE(player_->element_animations());
  EXPECT_EQ(player_->element_id(), element_id_);

  timeline_->AttachPlayer(player_);
  EXPECT_EQ(timeline_, player_->animation_timeline());
  EXPECT_EQ(player_, GetPlayerForLayerId(element_id_));
  EXPECT_TRUE(player_->element_animations());
  EXPECT_EQ(player_->element_id(), element_id_);

  // Removing player from timeline detaches layer.
  timeline_->DetachPlayer(player_);
  EXPECT_FALSE(player_->animation_timeline());
  EXPECT_FALSE(GetPlayerForLayerId(element_id_));
  EXPECT_FALSE(player_->element_animations());
  EXPECT_FALSE(player_->element_id());
}

TEST_F(AnimationPlayerTest, PropertiesMutate) {
  client_.RegisterLayer(element_id_, LayerTreeType::ACTIVE);
  client_impl_.RegisterLayer(element_id_, LayerTreeType::PENDING);
  client_impl_.RegisterLayer(element_id_, LayerTreeType::ACTIVE);

  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(player_);
  player_->AttachLayer(element_id_);

  const float start_opacity = .7f;
  const float end_opacity = .3f;

  const float start_brightness = .6f;
  const float end_brightness = .4f;

  const int transform_x = 10;
  const int transform_y = 20;

  const double duration = 1.;

  AddOpacityTransitionToPlayer(player_.get(), duration, start_opacity,
                               end_opacity, false);
  AddAnimatedTransformToPlayer(player_.get(), duration, transform_x,
                               transform_y);
  AddAnimatedFilterToPlayer(player_.get(), duration, start_brightness,
                            end_brightness);

  host_->PushPropertiesTo(host_impl_);

  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, LayerTreeType::ACTIVE,
                                         TargetProperty::OPACITY));
  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, LayerTreeType::ACTIVE,
                                         TargetProperty::TRANSFORM));
  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, LayerTreeType::ACTIVE,
                                         TargetProperty::FILTER));

  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, LayerTreeType::ACTIVE, TargetProperty::OPACITY));
  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, LayerTreeType::ACTIVE, TargetProperty::TRANSFORM));
  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, LayerTreeType::ACTIVE, TargetProperty::FILTER));

  host_impl_->ActivateAnimations();

  base::TimeTicks time;
  time += base::TimeDelta::FromSecondsD(0.1);
  AnimateLayersTransferEvents(time, 3u);

  time += base::TimeDelta::FromSecondsD(duration);
  AnimateLayersTransferEvents(time, 3u);

  client_.ExpectOpacityPropertyMutated(element_id_, LayerTreeType::ACTIVE,
                                       end_opacity);
  client_.ExpectTransformPropertyMutated(element_id_, LayerTreeType::ACTIVE,
                                         transform_x, transform_y);
  client_.ExpectFilterPropertyMutated(element_id_, LayerTreeType::ACTIVE,
                                      end_brightness);

  client_impl_.ExpectOpacityPropertyMutated(element_id_, LayerTreeType::ACTIVE,
                                            end_opacity);
  client_impl_.ExpectTransformPropertyMutated(
      element_id_, LayerTreeType::ACTIVE, transform_x, transform_y);
  client_impl_.ExpectFilterPropertyMutated(element_id_, LayerTreeType::ACTIVE,
                                           end_brightness);

  client_impl_.ExpectOpacityPropertyMutated(element_id_, LayerTreeType::PENDING,
                                            end_opacity);
  client_impl_.ExpectTransformPropertyMutated(
      element_id_, LayerTreeType::PENDING, transform_x, transform_y);
  client_impl_.ExpectFilterPropertyMutated(element_id_, LayerTreeType::PENDING,
                                           end_brightness);
}

TEST_F(AnimationPlayerTest, AttachTwoPlayersToOneLayer) {
  TestAnimationDelegate delegate1;
  TestAnimationDelegate delegate2;

  client_.RegisterLayer(element_id_, LayerTreeType::ACTIVE);
  client_impl_.RegisterLayer(element_id_, LayerTreeType::PENDING);
  client_impl_.RegisterLayer(element_id_, LayerTreeType::ACTIVE);

  scoped_refptr<AnimationPlayer> player1 =
      AnimationPlayer::Create(AnimationIdProvider::NextPlayerId());
  scoped_refptr<AnimationPlayer> player2 =
      AnimationPlayer::Create(AnimationIdProvider::NextPlayerId());

  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(player1);
  timeline_->AttachPlayer(player2);

  player1->set_layer_animation_delegate(&delegate1);
  player2->set_layer_animation_delegate(&delegate2);

  // Attach players to the same layer.
  player1->AttachLayer(element_id_);
  player2->AttachLayer(element_id_);

  const float start_opacity = .7f;
  const float end_opacity = .3f;

  const int transform_x = 10;
  const int transform_y = 20;

  const double duration = 1.;

  AddOpacityTransitionToPlayer(player1.get(), duration, start_opacity,
                               end_opacity, false);
  AddAnimatedTransformToPlayer(player2.get(), duration, transform_x,
                               transform_y);

  host_->PushPropertiesTo(host_impl_);
  host_impl_->ActivateAnimations();

  EXPECT_FALSE(delegate1.started());
  EXPECT_FALSE(delegate1.finished());

  EXPECT_FALSE(delegate2.started());
  EXPECT_FALSE(delegate2.finished());

  base::TimeTicks time;
  time += base::TimeDelta::FromSecondsD(0.1);
  AnimateLayersTransferEvents(time, 2u);

  EXPECT_TRUE(delegate1.started());
  EXPECT_FALSE(delegate1.finished());

  EXPECT_TRUE(delegate2.started());
  EXPECT_FALSE(delegate2.finished());

  time += base::TimeDelta::FromSecondsD(duration);
  AnimateLayersTransferEvents(time, 2u);

  EXPECT_TRUE(delegate1.finished());
  EXPECT_TRUE(delegate2.finished());

  client_.ExpectOpacityPropertyMutated(element_id_, LayerTreeType::ACTIVE,
                                       end_opacity);
  client_.ExpectTransformPropertyMutated(element_id_, LayerTreeType::ACTIVE,
                                         transform_x, transform_y);

  client_impl_.ExpectOpacityPropertyMutated(element_id_, LayerTreeType::ACTIVE,
                                            end_opacity);
  client_impl_.ExpectTransformPropertyMutated(
      element_id_, LayerTreeType::ACTIVE, transform_x, transform_y);

  client_impl_.ExpectOpacityPropertyMutated(element_id_, LayerTreeType::PENDING,
                                            end_opacity);
  client_impl_.ExpectTransformPropertyMutated(
      element_id_, LayerTreeType::PENDING, transform_x, transform_y);
}

TEST_F(AnimationPlayerTest, AddRemoveAnimationToNonAttachedPlayer) {
  client_.RegisterLayer(element_id_, LayerTreeType::ACTIVE);
  client_impl_.RegisterLayer(element_id_, LayerTreeType::PENDING);
  client_impl_.RegisterLayer(element_id_, LayerTreeType::ACTIVE);

  const double duration = 1.;
  const float start_opacity = .7f;
  const float end_opacity = .3f;

  const int filter_id =
      AddAnimatedFilterToPlayer(player_.get(), duration, 0.1f, 0.9f);
  const int opacity_id = AddOpacityTransitionToPlayer(
      player_.get(), duration, start_opacity, end_opacity, false);

  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(player_);

  EXPECT_FALSE(player_->element_animations());
  player_->RemoveAnimation(filter_id);

  player_->AttachLayer(element_id_);

  EXPECT_TRUE(player_->element_animations());
  EXPECT_FALSE(player_->element_animations()
                   ->GetAnimationById(filter_id));
  EXPECT_TRUE(player_->element_animations()
                  ->GetAnimationById(opacity_id));

  host_->PushPropertiesTo(host_impl_);

  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, LayerTreeType::ACTIVE,
                                         TargetProperty::OPACITY));
  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, LayerTreeType::ACTIVE, TargetProperty::OPACITY));

  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, LayerTreeType::ACTIVE,
                                         TargetProperty::FILTER));
  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, LayerTreeType::ACTIVE, TargetProperty::FILTER));

  host_impl_->ActivateAnimations();

  base::TimeTicks time;
  time += base::TimeDelta::FromSecondsD(0.1);
  AnimateLayersTransferEvents(time, 1u);

  time += base::TimeDelta::FromSecondsD(duration);
  AnimateLayersTransferEvents(time, 1u);

  client_.ExpectOpacityPropertyMutated(element_id_, LayerTreeType::ACTIVE,
                                       end_opacity);
  client_impl_.ExpectOpacityPropertyMutated(element_id_, LayerTreeType::ACTIVE,
                                            end_opacity);
  client_impl_.ExpectOpacityPropertyMutated(element_id_, LayerTreeType::PENDING,
                                            end_opacity);

  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, LayerTreeType::ACTIVE,
                                         TargetProperty::FILTER));
  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, LayerTreeType::ACTIVE, TargetProperty::FILTER));
}

TEST_F(AnimationPlayerTest, AddRemoveAnimationCausesSetNeedsCommit) {
  client_.RegisterLayer(element_id_, LayerTreeType::ACTIVE);
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(player_);
  player_->AttachLayer(element_id_);

  EXPECT_FALSE(client_.mutators_need_commit());

  const int animation_id =
      AddOpacityTransitionToPlayer(player_.get(), 1., .7f, .3f, false);

  EXPECT_TRUE(client_.mutators_need_commit());
  client_.set_mutators_need_commit(false);

  player_->PauseAnimation(animation_id, 1.);
  EXPECT_TRUE(client_.mutators_need_commit());
  client_.set_mutators_need_commit(false);

  player_->RemoveAnimation(animation_id);
  EXPECT_TRUE(client_.mutators_need_commit());
  client_.set_mutators_need_commit(false);
}

// If main-thread player switches to another layer within one frame then
// impl-thread player must be switched as well.
TEST_F(AnimationPlayerTest, SwitchToLayer) {
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(player_);
  player_->AttachLayer(element_id_);

  host_->PushPropertiesTo(host_impl_);

  GetImplTimelineAndPlayerByID();

  EXPECT_EQ(player_, GetPlayerForLayerId(element_id_));
  EXPECT_TRUE(player_->element_animations());
  EXPECT_EQ(player_->element_id(), element_id_);

  EXPECT_EQ(player_impl_, GetImplPlayerForLayerId(element_id_));
  EXPECT_TRUE(player_impl_->element_animations());
  EXPECT_EQ(player_impl_->element_id(), element_id_);

  const int new_layer_id = NextTestLayerId();
  player_->DetachLayer();
  player_->AttachLayer(new_layer_id);

  EXPECT_EQ(player_, GetPlayerForLayerId(new_layer_id));
  EXPECT_TRUE(player_->element_animations());
  EXPECT_EQ(player_->element_id(), new_layer_id);

  host_->PushPropertiesTo(host_impl_);

  EXPECT_EQ(player_impl_, GetImplPlayerForLayerId(new_layer_id));
  EXPECT_TRUE(player_impl_->element_animations());
  EXPECT_EQ(player_impl_->element_id(), new_layer_id);
}

}  // namespace
}  // namespace cc
