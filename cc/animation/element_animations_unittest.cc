// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/element_animations.h"

#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/animation_timeline.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/animation_timelines_test_common.h"

namespace cc {
namespace {

class ElementAnimationsTest : public AnimationTimelinesTest {
 public:
  ElementAnimationsTest() {}
  ~ElementAnimationsTest() override {}
};

// See animation_player_unittest.cc for integration with AnimationPlayer.

TEST_F(ElementAnimationsTest, AttachToLayerInActiveTree) {
  // Set up the layer which is in active tree for main thread and not
  // yet passed onto the impl thread.
  client_.RegisterLayer(layer_id_, LayerTreeType::ACTIVE);
  client_impl_.RegisterLayer(layer_id_, LayerTreeType::PENDING);

  EXPECT_TRUE(client_.IsLayerInTree(layer_id_, LayerTreeType::ACTIVE));
  EXPECT_FALSE(client_.IsLayerInTree(layer_id_, LayerTreeType::PENDING));

  host_->AddAnimationTimeline(timeline_);

  timeline_->AttachPlayer(player_);
  player_->AttachLayer(layer_id_);

  scoped_refptr<ElementAnimations> element_animations =
      player_->element_animations();
  EXPECT_TRUE(element_animations);

  EXPECT_TRUE(element_animations->needs_active_value_observations());
  EXPECT_FALSE(element_animations->needs_pending_value_observations());

  host_->PushPropertiesTo(host_impl_);

  GetImplTimelineAndPlayerByID();

  scoped_refptr<ElementAnimations> element_animations_impl =
      player_impl_->element_animations();
  EXPECT_TRUE(element_animations_impl);

  EXPECT_FALSE(element_animations_impl->needs_active_value_observations());
  EXPECT_TRUE(element_animations_impl->needs_pending_value_observations());

  // Create the layer in the impl active tree.
  client_impl_.RegisterLayer(layer_id_, LayerTreeType::ACTIVE);
  EXPECT_TRUE(element_animations_impl->needs_active_value_observations());
  EXPECT_TRUE(element_animations_impl->needs_pending_value_observations());

  EXPECT_TRUE(client_impl_.IsLayerInTree(layer_id_, LayerTreeType::ACTIVE));
  EXPECT_TRUE(client_impl_.IsLayerInTree(layer_id_, LayerTreeType::PENDING));

  // kill layer on main thread.
  client_.UnregisterLayer(layer_id_, LayerTreeType::ACTIVE);
  EXPECT_EQ(element_animations, player_->element_animations());
  EXPECT_FALSE(element_animations->needs_active_value_observations());
  EXPECT_FALSE(element_animations->needs_pending_value_observations());

  // Sync doesn't detach LayerImpl.
  host_->PushPropertiesTo(host_impl_);
  EXPECT_EQ(element_animations_impl, player_impl_->element_animations());
  EXPECT_TRUE(element_animations_impl->needs_active_value_observations());
  EXPECT_TRUE(element_animations_impl->needs_pending_value_observations());

  // Kill layer on impl thread in pending tree.
  client_impl_.UnregisterLayer(layer_id_, LayerTreeType::PENDING);
  EXPECT_EQ(element_animations_impl, player_impl_->element_animations());
  EXPECT_TRUE(element_animations_impl->needs_active_value_observations());
  EXPECT_FALSE(element_animations_impl->needs_pending_value_observations());

  // Kill layer on impl thread in active tree.
  client_impl_.UnregisterLayer(layer_id_, LayerTreeType::ACTIVE);
  EXPECT_EQ(element_animations_impl, player_impl_->element_animations());
  EXPECT_FALSE(element_animations_impl->needs_active_value_observations());
  EXPECT_FALSE(element_animations_impl->needs_pending_value_observations());

  // Sync doesn't change anything.
  host_->PushPropertiesTo(host_impl_);
  EXPECT_EQ(element_animations_impl, player_impl_->element_animations());
  EXPECT_FALSE(element_animations_impl->needs_active_value_observations());
  EXPECT_FALSE(element_animations_impl->needs_pending_value_observations());

  player_->DetachLayer();
  EXPECT_FALSE(player_->element_animations());

  // Release ptrs now to test the order of destruction.
  ReleaseRefPtrs();
}

TEST_F(ElementAnimationsTest, AttachToNotYetCreatedLayer) {
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(player_);

  host_->PushPropertiesTo(host_impl_);

  GetImplTimelineAndPlayerByID();

  player_->AttachLayer(layer_id_);

  scoped_refptr<ElementAnimations> element_animations =
      player_->element_animations();
  EXPECT_TRUE(element_animations);

  EXPECT_FALSE(element_animations->needs_active_value_observations());
  EXPECT_FALSE(element_animations->needs_pending_value_observations());

  host_->PushPropertiesTo(host_impl_);

  scoped_refptr<ElementAnimations> element_animations_impl =
      player_impl_->element_animations();
  EXPECT_TRUE(element_animations_impl);

  EXPECT_FALSE(element_animations_impl->needs_active_value_observations());
  EXPECT_FALSE(element_animations_impl->needs_pending_value_observations());

  // Create layer.
  client_.RegisterLayer(layer_id_, LayerTreeType::ACTIVE);
  EXPECT_TRUE(element_animations->needs_active_value_observations());
  EXPECT_FALSE(element_animations->needs_pending_value_observations());

  client_impl_.RegisterLayer(layer_id_, LayerTreeType::PENDING);
  EXPECT_FALSE(element_animations_impl->needs_active_value_observations());
  EXPECT_TRUE(element_animations_impl->needs_pending_value_observations());

  client_impl_.RegisterLayer(layer_id_, LayerTreeType::ACTIVE);
  EXPECT_TRUE(element_animations_impl->needs_active_value_observations());
  EXPECT_TRUE(element_animations_impl->needs_pending_value_observations());
}

TEST_F(ElementAnimationsTest, AddRemovePlayers) {
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(player_);
  player_->AttachLayer(layer_id_);

  scoped_refptr<ElementAnimations> element_animations =
      player_->element_animations();
  EXPECT_TRUE(element_animations);

  scoped_refptr<AnimationPlayer> player1 =
      AnimationPlayer::Create(AnimationIdProvider::NextPlayerId());
  scoped_refptr<AnimationPlayer> player2 =
      AnimationPlayer::Create(AnimationIdProvider::NextPlayerId());

  timeline_->AttachPlayer(player1);
  timeline_->AttachPlayer(player2);

  // Attach players to the same layer.
  player1->AttachLayer(layer_id_);
  player2->AttachLayer(layer_id_);

  EXPECT_EQ(element_animations, player1->element_animations());
  EXPECT_EQ(element_animations, player2->element_animations());

  host_->PushPropertiesTo(host_impl_);
  GetImplTimelineAndPlayerByID();

  scoped_refptr<ElementAnimations> element_animations_impl =
      player_impl_->element_animations();
  EXPECT_TRUE(element_animations_impl);

  int list_size_before = 0;
  for (const ElementAnimations::PlayersListNode* node =
           element_animations_impl->players_list().head();
       node != element_animations_impl->players_list().end();
       node = node->next()) {
    const AnimationPlayer* player_impl = node->value();
    EXPECT_TRUE(timeline_->GetPlayerById(player_impl->id()));
    ++list_size_before;
  }
  EXPECT_EQ(3, list_size_before);

  player2->DetachLayer();
  EXPECT_FALSE(player2->element_animations());
  EXPECT_EQ(element_animations, player_->element_animations());
  EXPECT_EQ(element_animations, player1->element_animations());

  host_->PushPropertiesTo(host_impl_);
  EXPECT_EQ(element_animations_impl, player_impl_->element_animations());

  int list_size_after = 0;
  for (const ElementAnimations::PlayersListNode* node =
           element_animations_impl->players_list().head();
       node != element_animations_impl->players_list().end();
       node = node->next()) {
    const AnimationPlayer* player_impl = node->value();
    EXPECT_TRUE(timeline_->GetPlayerById(player_impl->id()));
    ++list_size_after;
  }
  EXPECT_EQ(2, list_size_after);
}

}  // namespace
}  // namespace cc
