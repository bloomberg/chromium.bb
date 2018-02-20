// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/element_animations.h"

#include "base/memory/ptr_util.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/single_keyframe_effect_animation_player.h"
#include "cc/animation/transform_operations.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/animation_timelines_test_common.h"
#include "ui/gfx/geometry/box_f.h"

namespace cc {
namespace {

using base::TimeDelta;
using base::TimeTicks;

static base::TimeTicks TicksFromSecondsF(double seconds) {
  return base::TimeTicks() + base::TimeDelta::FromSecondsD(seconds);
}

// An ElementAnimations cannot be ticked at 0.0, since an animation
// with start time 0.0 is treated as an animation whose start time has
// not yet been set.
const TimeTicks kInitialTickTime = TicksFromSecondsF(1.0);

class ElementAnimationsTest : public AnimationTimelinesTest {
 public:
  ElementAnimationsTest() = default;
  ~ElementAnimationsTest() override = default;

  void SetUp() override {
    AnimationTimelinesTest::SetUp();
    player_ = SingleKeyframeEffectAnimationPlayer::Create(player_id_);
  }

  void CreateImplTimelineAndPlayer() override {
    AnimationTimelinesTest::CreateImplTimelineAndPlayer();
  }

  std::unique_ptr<AnimationEvents> CreateEventsForTesting() {
    auto mutator_events = host_impl_->CreateEvents();
    return base::WrapUnique(
        static_cast<AnimationEvents*>(mutator_events.release()));
  }
};

// See animation_player_unittest.cc for integration with AnimationPlayer.

TEST_F(ElementAnimationsTest, AttachToLayerInActiveTree) {
  // Set up the layer which is in active tree for main thread and not
  // yet passed onto the impl thread.
  client_.RegisterElement(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElement(element_id_, ElementListType::PENDING);

  EXPECT_TRUE(client_.IsElementInList(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.IsElementInList(element_id_, ElementListType::PENDING));

  AttachTimelinePlayerLayer();

  EXPECT_TRUE(element_animations_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_->has_element_in_pending_list());

  PushProperties();

  GetImplTimelineAndPlayerByID();

  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());

  // Create the layer in the impl active tree.
  client_impl_.RegisterElement(element_id_, ElementListType::ACTIVE);
  EXPECT_TRUE(element_animations_impl_->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());

  EXPECT_TRUE(
      client_impl_.IsElementInList(element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(
      client_impl_.IsElementInList(element_id_, ElementListType::PENDING));

  // kill layer on main thread.
  client_.UnregisterElement(element_id_, ElementListType::ACTIVE);
  EXPECT_EQ(element_animations_,
            player_->keyframe_effect()->element_animations());
  EXPECT_FALSE(element_animations_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_->has_element_in_pending_list());

  // Sync doesn't detach LayerImpl.
  PushProperties();
  EXPECT_EQ(element_animations_impl_,
            player_impl_->keyframe_effect()->element_animations());
  EXPECT_TRUE(element_animations_impl_->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());

  // Kill layer on impl thread in pending tree.
  client_impl_.UnregisterElement(element_id_, ElementListType::PENDING);
  EXPECT_EQ(element_animations_impl_,
            player_impl_->keyframe_effect()->element_animations());
  EXPECT_TRUE(element_animations_impl_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_impl_->has_element_in_pending_list());

  // Kill layer on impl thread in active tree.
  client_impl_.UnregisterElement(element_id_, ElementListType::ACTIVE);
  EXPECT_EQ(element_animations_impl_,
            player_impl_->keyframe_effect()->element_animations());
  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_impl_->has_element_in_pending_list());

  // Sync doesn't change anything.
  PushProperties();
  EXPECT_EQ(element_animations_impl_,
            player_impl_->keyframe_effect()->element_animations());
  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_impl_->has_element_in_pending_list());

  player_->DetachElement();
  EXPECT_FALSE(player_->keyframe_effect()->element_animations());

  // Release ptrs now to test the order of destruction.
  ReleaseRefPtrs();
}

TEST_F(ElementAnimationsTest, AttachToNotYetCreatedLayer) {
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(player_);

  PushProperties();
  GetImplTimelineAndPlayerByID();

  // Perform attachment separately.
  player_->AttachElement(element_id_);
  element_animations_ = player_->keyframe_effect()->element_animations();

  EXPECT_FALSE(element_animations_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_->has_element_in_pending_list());

  PushProperties();
  element_animations_impl_ =
      player_impl_->keyframe_effect()->element_animations();

  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_impl_->has_element_in_pending_list());

  // Create layer.
  client_.RegisterElement(element_id_, ElementListType::ACTIVE);
  EXPECT_TRUE(element_animations_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_->has_element_in_pending_list());

  client_impl_.RegisterElement(element_id_, ElementListType::PENDING);
  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());

  client_impl_.RegisterElement(element_id_, ElementListType::ACTIVE);
  EXPECT_TRUE(element_animations_impl_->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());
}

TEST_F(ElementAnimationsTest, AddRemovePlayers) {
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(player_);
  player_->AttachElement(element_id_);

  scoped_refptr<ElementAnimations> element_animations =
      player_->keyframe_effect()->element_animations();
  EXPECT_TRUE(element_animations);

  scoped_refptr<SingleKeyframeEffectAnimationPlayer> player1 =
      SingleKeyframeEffectAnimationPlayer::Create(
          AnimationIdProvider::NextPlayerId());
  scoped_refptr<SingleKeyframeEffectAnimationPlayer> player2 =
      SingleKeyframeEffectAnimationPlayer::Create(
          AnimationIdProvider::NextPlayerId());

  timeline_->AttachPlayer(player1);
  timeline_->AttachPlayer(player2);

  // Attach players to the same layer.
  player1->AttachElement(element_id_);
  player2->AttachElement(element_id_);

  EXPECT_EQ(element_animations,
            player1->keyframe_effect()->element_animations());
  EXPECT_EQ(element_animations,
            player2->keyframe_effect()->element_animations());

  PushProperties();
  GetImplTimelineAndPlayerByID();

  scoped_refptr<ElementAnimations> element_animations_impl =
      player_impl_->keyframe_effect()->element_animations();
  EXPECT_TRUE(element_animations_impl);

  const ElementAnimations::KeyframeEffectsList& keyframe_effects =
      element_animations_impl_->keyframe_effects_list();
  int list_size_before = 0;
  for (auto it = keyframe_effects.begin(); it != keyframe_effects.end(); ++it)
    ++list_size_before;
  EXPECT_EQ(3, list_size_before);

  player2->DetachElement();
  EXPECT_FALSE(player2->keyframe_effect()->element_animations());
  EXPECT_EQ(element_animations,
            player_->keyframe_effect()->element_animations());
  EXPECT_EQ(element_animations,
            player1->keyframe_effect()->element_animations());

  PushProperties();
  EXPECT_EQ(element_animations_impl,
            player_impl_->keyframe_effect()->element_animations());

  int list_size_after = 0;
  for (auto it = keyframe_effects.begin(); it != keyframe_effects.end(); ++it)
    ++list_size_after;
  EXPECT_EQ(2, list_size_after);
}

TEST_F(ElementAnimationsTest, SyncNewAnimation) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  EXPECT_FALSE(player_->GetKeyframeModel(TargetProperty::OPACITY));
  EXPECT_FALSE(player_impl_->GetKeyframeModel(TargetProperty::OPACITY));

  int keyframe_model_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0, 1, false);
  EXPECT_TRUE(
      player_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            player_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  EXPECT_FALSE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));

  PushProperties();
  player_impl_->ActivateKeyframeEffects();

  EXPECT_TRUE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
}

TEST_F(ElementAnimationsTest,
       SyncScrollOffsetAnimationRespectsHasSetInitialValue) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  EXPECT_FALSE(player_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET));
  EXPECT_FALSE(player_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET));

  gfx::ScrollOffset initial_value(100.f, 300.f);
  gfx::ScrollOffset provider_initial_value(150.f, 300.f);
  gfx::ScrollOffset target_value(300.f, 200.f);

  client_impl_.SetScrollOffsetForAnimation(provider_initial_value);

  // Animation with initial value set.
  std::unique_ptr<ScrollOffsetAnimationCurve> curve_fixed(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
  curve_fixed->SetInitialValue(initial_value);
  const int animation1_id = 1;
  std::unique_ptr<KeyframeModel> animation_fixed(KeyframeModel::Create(
      std::move(curve_fixed), animation1_id, 0, TargetProperty::SCROLL_OFFSET));
  player_->AddKeyframeModel(std::move(animation_fixed));
  PushProperties();
  EXPECT_VECTOR2DF_EQ(initial_value, player_impl_->keyframe_effect()
                                         ->GetKeyframeModelById(animation1_id)
                                         ->curve()
                                         ->ToScrollOffsetAnimationCurve()
                                         ->GetValue(base::TimeDelta()));

  // Animation without initial value set.
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
  const int animation2_id = 2;
  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), animation2_id, 0, TargetProperty::SCROLL_OFFSET));
  player_->AddKeyframeModel(std::move(keyframe_model));
  PushProperties();
  EXPECT_VECTOR2DF_EQ(provider_initial_value,
                      player_impl_->keyframe_effect()
                          ->GetKeyframeModelById(animation2_id)
                          ->curve()
                          ->ToScrollOffsetAnimationCurve()
                          ->GetValue(base::TimeDelta()));
}

class TestAnimationDelegateThatDestroysPlayer : public TestAnimationDelegate {
 public:
  TestAnimationDelegateThatDestroysPlayer() = default;

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override {
    TestAnimationDelegate::NotifyAnimationStarted(monotonic_time,
                                                  target_property, group);
    // Detaching player from the timeline ensures that the timeline doesn't hold
    // a reference to the player and the player is destroyed.
    timeline_->DetachPlayer(player_);
  };

  void setTimelineAndPlayer(
      scoped_refptr<AnimationTimeline> timeline,
      scoped_refptr<SingleKeyframeEffectAnimationPlayer> player) {
    timeline_ = timeline;
    player_ = player;
  }

 private:
  scoped_refptr<AnimationTimeline> timeline_;
  scoped_refptr<SingleKeyframeEffectAnimationPlayer> player_;
};

// Test that we don't crash if a player is deleted while ElementAnimations is
// iterating through the list of players (see crbug.com/631052). This test
// passes if it doesn't crash.
TEST_F(ElementAnimationsTest, AddedPlayerIsDestroyed) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  TestAnimationDelegateThatDestroysPlayer delegate;

  const int player2_id = AnimationIdProvider::NextPlayerId();
  scoped_refptr<SingleKeyframeEffectAnimationPlayer> player2 =
      SingleKeyframeEffectAnimationPlayer::Create(player2_id);
  delegate.setTimelineAndPlayer(timeline_, player2);

  timeline_->AttachPlayer(player2);
  player2->AttachElement(element_id_);
  player2->set_animation_delegate(&delegate);

  int keyframe_model_id =
      AddOpacityTransitionToPlayer(player2.get(), 1.0, 0.f, 1.f, false);

  PushProperties();

  scoped_refptr<SingleKeyframeEffectAnimationPlayer> player2_impl =
      (SingleKeyframeEffectAnimationPlayer*)timeline_impl_->GetPlayerById(
          player2_id);
  DCHECK(player2_impl);

  player2_impl->ActivateKeyframeEffects();
  EXPECT_TRUE(
      player2_impl->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));

  player2_impl->Tick(kInitialTickTime);

  auto events = CreateEventsForTesting();
  player2_impl->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);

  // The actual detachment happens here, inside the callback
  player2->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);
  EXPECT_TRUE(delegate.started());
}

// If an animation is started on the impl thread before it is ticked on the main
// thread, we must be sure to respect the synchronized start time.
TEST_F(ElementAnimationsTest, DoNotClobberStartTimes) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  EXPECT_FALSE(player_impl_->GetKeyframeModel(TargetProperty::OPACITY));

  int keyframe_model_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0, 1, false);

  PushProperties();
  player_impl_->ActivateKeyframeEffects();

  EXPECT_TRUE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());

  auto events = CreateEventsForTesting();
  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());

  // Synchronize the start times.
  EXPECT_EQ(1u, events->events_.size());
  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);
  EXPECT_EQ(player_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->start_time(),
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->start_time());

  // Start the animation on the main thread. Should not affect the start time.
  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  player_impl_->UpdateState(true, nullptr);
  EXPECT_EQ(player_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->start_time(),
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->start_time());
}

TEST_F(ElementAnimationsTest, UseSpecifiedStartTimes) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  int keyframe_model_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0, 1, false);

  const TimeTicks start_time = TicksFromSecondsF(123);
  player_->GetKeyframeModel(TargetProperty::OPACITY)
      ->set_start_time(start_time);

  PushProperties();
  player_impl_->ActivateKeyframeEffects();

  EXPECT_TRUE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());

  auto events = CreateEventsForTesting();
  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());

  // Synchronize the start times.
  EXPECT_EQ(1u, events->events_.size());
  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);

  EXPECT_EQ(start_time, player_->keyframe_effect()
                            ->GetKeyframeModelById(keyframe_model_id)
                            ->start_time());
  EXPECT_EQ(player_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->start_time(),
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->start_time());

  // Start the animation on the main thread. Should not affect the start time.
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  player_->UpdateState(true, nullptr);
  EXPECT_EQ(start_time, player_->keyframe_effect()
                            ->GetKeyframeModelById(keyframe_model_id)
                            ->start_time());
  EXPECT_EQ(player_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->start_time(),
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->start_time());
}

// Tests that animationss activate and deactivate as expected.
TEST_F(ElementAnimationsTest, Activation) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  AnimationHost* host = client_.host();
  AnimationHost* host_impl = client_impl_.host();

  auto events = CreateEventsForTesting();

  EXPECT_EQ(1u, host->element_animations_for_testing().size());
  EXPECT_EQ(1u, host_impl->element_animations_for_testing().size());

  // Initially, both animationss should be inactive.
  EXPECT_EQ(0u, host->ticking_players_for_testing().size());
  EXPECT_EQ(0u, host_impl->ticking_players_for_testing().size());

  AddOpacityTransitionToPlayer(player_.get(), 1, 0, 1, false);
  // The main thread animations should now be active.
  EXPECT_EQ(1u, host->ticking_players_for_testing().size());

  PushProperties();
  player_impl_->ActivateKeyframeEffects();
  // Both animationss should now be active.
  EXPECT_EQ(1u, host->ticking_players_for_testing().size());
  EXPECT_EQ(1u, host_impl->ticking_players_for_testing().size());

  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);

  EXPECT_EQ(1u, host->ticking_players_for_testing().size());
  EXPECT_EQ(1u, host_impl->ticking_players_for_testing().size());

  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  player_->UpdateState(true, nullptr);
  EXPECT_EQ(1u, host->ticking_players_for_testing().size());

  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_->UpdateState(true, nullptr);
  EXPECT_EQ(KeyframeModel::FINISHED,
            player_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  EXPECT_EQ(1u, host->ticking_players_for_testing().size());

  events = CreateEventsForTesting();

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1500));
  player_impl_->UpdateState(true, events.get());

  EXPECT_EQ(
      KeyframeModel::WAITING_FOR_DELETION,
      player_impl_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  // The impl thread animations should have de-activated.
  EXPECT_EQ(0u, host_impl->ticking_players_for_testing().size());

  EXPECT_EQ(1u, events->events_.size());
  player_->keyframe_effect()->NotifyKeyframeModelFinished(events->events_[0]);
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1500));
  player_->UpdateState(true, nullptr);

  EXPECT_EQ(KeyframeModel::WAITING_FOR_DELETION,
            player_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  // The main thread animations should have de-activated.
  EXPECT_EQ(0u, host->ticking_players_for_testing().size());

  PushProperties();
  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(player_->keyframe_effect()->has_any_keyframe_model());
  EXPECT_FALSE(player_impl_->keyframe_effect()->has_any_keyframe_model());
  EXPECT_EQ(0u, host->ticking_players_for_testing().size());
  EXPECT_EQ(0u, host_impl->ticking_players_for_testing().size());
}

TEST_F(ElementAnimationsTest, SyncPause) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  EXPECT_FALSE(player_impl_->GetKeyframeModel(TargetProperty::OPACITY));

  // Two steps, three ranges: [0-1) -> 0.2, [1-2) -> 0.3, [2-3] -> 0.4.
  const double duration = 3.0;
  const int keyframe_model_id =
      AddOpacityStepsToPlayer(player_.get(), duration, 0.2f, 0.4f, 2);

  // Set start offset to be at the beginning of the second range.
  player_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->set_time_offset(TimeDelta::FromSecondsD(1.01));

  PushProperties();
  player_impl_->ActivateKeyframeEffects();

  EXPECT_TRUE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());

  TimeTicks time = kInitialTickTime;

  // Start the animations on each animations.
  auto events = CreateEventsForTesting();
  player_impl_->Tick(time);
  player_impl_->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());

  player_->Tick(time);
  player_->UpdateState(true, nullptr);
  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);

  EXPECT_EQ(KeyframeModel::RUNNING,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  EXPECT_EQ(KeyframeModel::RUNNING,
            player_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());

  EXPECT_EQ(0.3f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_EQ(0.3f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_EQ(kInitialTickTime, player_->keyframe_effect()
                                  ->GetKeyframeModelById(keyframe_model_id)
                                  ->start_time());
  EXPECT_EQ(kInitialTickTime, player_impl_->keyframe_effect()
                                  ->GetKeyframeModelById(keyframe_model_id)
                                  ->start_time());

  // Pause the animation at the middle of the second range so the offset
  // delays animation until the middle of the third range.
  player_->PauseKeyframeModel(keyframe_model_id, 1.5);
  EXPECT_EQ(KeyframeModel::PAUSED, player_->keyframe_effect()
                                       ->GetKeyframeModelById(keyframe_model_id)
                                       ->run_state());

  // The pause run state change should make it to the impl thread animations.
  PushProperties();
  player_impl_->ActivateKeyframeEffects();

  // Advance time so it stays within the first range.
  time += TimeDelta::FromMilliseconds(10);
  player_->Tick(time);
  player_impl_->Tick(time);

  EXPECT_EQ(KeyframeModel::PAUSED, player_impl_->keyframe_effect()
                                       ->GetKeyframeModelById(keyframe_model_id)
                                       ->run_state());

  // Opacity value doesn't depend on time if paused at specified time offset.
  EXPECT_EQ(0.4f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_EQ(0.4f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, DoNotSyncFinishedAnimation) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  EXPECT_FALSE(player_impl_->GetKeyframeModel(TargetProperty::OPACITY));

  int keyframe_model_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0, 1, false);

  PushProperties();
  player_impl_->ActivateKeyframeEffects();

  EXPECT_TRUE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());

  events = CreateEventsForTesting();
  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);

  // Notify main thread animations that the animation has started.
  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);

  // Complete animation on impl thread.
  events = CreateEventsForTesting();
  player_impl_->Tick(kInitialTickTime + TimeDelta::FromSeconds(1));
  player_impl_->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[0].type);

  player_->keyframe_effect()->NotifyKeyframeModelFinished(events->events_[0]);

  player_->Tick(kInitialTickTime + TimeDelta::FromSeconds(2));
  player_->UpdateState(true, nullptr);

  PushProperties();
  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(
      player_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_FALSE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
}

// Ensure that a finished animation is eventually deleted by both the
// main-thread and the impl-thread animationss.
TEST_F(ElementAnimationsTest, AnimationsAreDeleted) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  AddOpacityTransitionToPlayer(player_.get(), 1.0, 0.0f, 1.0f, false);
  player_->Tick(kInitialTickTime);
  player_->UpdateState(true, nullptr);
  EXPECT_TRUE(player_->keyframe_effect()->needs_push_properties());

  PushProperties();
  EXPECT_FALSE(player_->keyframe_effect()->needs_push_properties());

  EXPECT_FALSE(host_->needs_push_properties());
  EXPECT_FALSE(host_impl_->needs_push_properties());

  player_impl_->ActivateKeyframeEffects();

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  player_impl_->UpdateState(true, events.get());

  // There should be a STARTED event for the animation.
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);
  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);

  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_->UpdateState(true, nullptr);

  EXPECT_FALSE(host_->needs_push_properties());
  EXPECT_FALSE(host_impl_->needs_push_properties());

  events = CreateEventsForTesting();
  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  player_impl_->UpdateState(true, events.get());

  EXPECT_TRUE(host_impl_->needs_push_properties());

  // There should be a FINISHED event for the animation.
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[0].type);

  // Neither animations should have deleted the animation yet.
  EXPECT_TRUE(player_->GetKeyframeModel(TargetProperty::OPACITY));
  EXPECT_TRUE(player_impl_->GetKeyframeModel(TargetProperty::OPACITY));

  player_->keyframe_effect()->NotifyKeyframeModelFinished(events->events_[0]);

  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(3000));
  player_->UpdateState(true, nullptr);
  EXPECT_TRUE(host_->needs_push_properties());

  PushProperties();

  // Both animationss should now have deleted the animation. The impl animations
  // should have deleted the animation even though activation has not occurred,
  // since the animation was already waiting for deletion when
  // PushPropertiesTo was called.
  EXPECT_FALSE(player_->keyframe_effect()->has_any_keyframe_model());
  EXPECT_FALSE(player_impl_->keyframe_effect()->has_any_keyframe_model());
}

// Tests that transitioning opacity from 0 to 1 works as expected.

static std::unique_ptr<KeyframeModel> CreateKeyframeModel(
    std::unique_ptr<AnimationCurve> curve,
    int group_id,
    TargetProperty::Type property) {
  return KeyframeModel::Create(std::move(curve), 0, group_id, property);
}

TEST_F(ElementAnimationsTest, TrivialTransition) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<KeyframeModel> to_add(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  int keyframe_model_id = to_add->id();

  EXPECT_FALSE(
      player_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  player_->AddKeyframeModel(std::move(to_add));
  EXPECT_TRUE(
      player_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            player_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  player_->Tick(kInitialTickTime);
  EXPECT_EQ(KeyframeModel::STARTING,
            player_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_->UpdateState(true, events.get());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->HasTickingKeyframeModel());
}

TEST_F(ElementAnimationsTest, FilterTransition) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());

  FilterOperations start_filters;
  start_filters.Append(FilterOperation::CreateBrightnessFilter(1.f));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::TimeDelta(), start_filters, nullptr));
  FilterOperations end_filters;
  end_filters.Append(FilterOperation::CreateBrightnessFilter(2.f));
  curve->AddKeyframe(FilterKeyframe::Create(base::TimeDelta::FromSecondsD(1.0),
                                            end_filters, nullptr));

  std::unique_ptr<KeyframeModel> keyframe_model(
      KeyframeModel::Create(std::move(curve), 1, 0, TargetProperty::FILTER));
  player_->AddKeyframeModel(std::move(keyframe_model));

  player_->Tick(kInitialTickTime);
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(start_filters,
            client_.GetFilters(element_id_, ElementListType::ACTIVE));

  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  player_->UpdateState(true, events.get());
  EXPECT_EQ(1u,
            client_.GetFilters(element_id_, ElementListType::ACTIVE).size());
  EXPECT_EQ(FilterOperation::CreateBrightnessFilter(1.5f),
            client_.GetFilters(element_id_, ElementListType::ACTIVE).at(0));

  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_->UpdateState(true, events.get());
  EXPECT_EQ(end_filters,
            client_.GetFilters(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->HasTickingKeyframeModel());
}

TEST_F(ElementAnimationsTest, ScrollOffsetTransition) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  gfx::ScrollOffset initial_value(100.f, 300.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));

  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), 1, 0, TargetProperty::SCROLL_OFFSET));
  keyframe_model->set_needs_synchronized_start_time(true);
  player_->AddKeyframeModel(std::move(keyframe_model));

  client_impl_.SetScrollOffsetForAnimation(initial_value);
  PushProperties();
  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(player_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET));
  TimeDelta duration =
      player_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
          ->curve()
          ->Duration();
  EXPECT_EQ(duration, player_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
                          ->curve()
                          ->Duration());

  player_->Tick(kInitialTickTime);
  player_->UpdateState(true, nullptr);
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(initial_value,
            client_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());
  EXPECT_TRUE(player_impl_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(initial_value,
            client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);
  player_->Tick(kInitialTickTime + duration / 2);
  player_->UpdateState(true, nullptr);
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(200.f, 250.f),
      client_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime + duration / 2);
  player_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(200.f, 250.f),
      client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime + duration);
  player_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(target_value, client_impl_.GetScrollOffset(
                                        element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_impl_->keyframe_effect()->HasTickingKeyframeModel());

  player_->Tick(kInitialTickTime + duration);
  player_->UpdateState(true, nullptr);
  EXPECT_VECTOR2DF_EQ(target_value, client_.GetScrollOffset(
                                        element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->HasTickingKeyframeModel());
}

TEST_F(ElementAnimationsTest, ScrollOffsetTransitionOnImplOnly) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  gfx::ScrollOffset initial_value(100.f, 300.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
  curve->SetInitialValue(initial_value);
  double duration_in_seconds = curve->Duration().InSecondsF();

  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), 1, 0, TargetProperty::SCROLL_OFFSET));
  keyframe_model->SetIsImplOnly();
  player_impl_->AddKeyframeModel(std::move(keyframe_model));

  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());
  EXPECT_TRUE(player_impl_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(initial_value,
            client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  TimeDelta duration = TimeDelta::FromMicroseconds(
      duration_in_seconds * base::Time::kMicrosecondsPerSecond);

  player_impl_->Tick(kInitialTickTime + duration / 2);
  player_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(200.f, 250.f),
      client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime + duration);
  player_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(target_value, client_impl_.GetScrollOffset(
                                        element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_impl_->keyframe_effect()->HasTickingKeyframeModel());
}

// This test verifies that if an animation is added after a layer is animated,
// it doesn't get promoted to be in the RUNNING state. This prevents cases where
// a start time gets set on an animation using the stale value of
// last_tick_time_.
TEST_F(ElementAnimationsTest, UpdateStateWithoutAnimate) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  // Add first scroll offset animation.
  AddScrollOffsetAnimationToPlayer(player_impl_.get(),
                                   gfx::ScrollOffset(100.f, 300.f),
                                   gfx::ScrollOffset(100.f, 200.f));

  // Calling UpdateState after Animate should promote the animation to running
  // state.
  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());
  EXPECT_EQ(KeyframeModel::RUNNING,
            player_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
                ->run_state());

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1500));
  player_impl_->UpdateState(true, events.get());
  EXPECT_EQ(nullptr,
            player_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET));

  // Add second scroll offset animation.
  AddScrollOffsetAnimationToPlayer(player_impl_.get(),
                                   gfx::ScrollOffset(100.f, 200.f),
                                   gfx::ScrollOffset(100.f, 100.f));

  // Calling UpdateState without Animate should NOT promote the animation to
  // running state.
  player_impl_->UpdateState(true, events.get());
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
                ->run_state());

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  player_impl_->UpdateState(true, events.get());

  EXPECT_EQ(KeyframeModel::RUNNING,
            player_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
                ->run_state());
  EXPECT_VECTOR2DF_EQ(
      gfx::ScrollOffset(100.f, 200.f),
      client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));
}

// Ensure that when the impl animations doesn't have a value provider,
// the main-thread animations's value provider is used to obtain the intial
// scroll offset.
TEST_F(ElementAnimationsTest, ScrollOffsetTransitionNoImplProvider) {
  CreateTestLayer(false, false);
  CreateTestImplLayer(ElementListType::PENDING);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());
  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());

  auto events = CreateEventsForTesting();

  gfx::ScrollOffset initial_value(500.f, 100.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));

  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), 1, 0, TargetProperty::SCROLL_OFFSET));
  keyframe_model->set_needs_synchronized_start_time(true);
  player_->AddKeyframeModel(std::move(keyframe_model));

  client_.SetScrollOffsetForAnimation(initial_value);
  PushProperties();
  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(player_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET));
  TimeDelta duration =
      player_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
          ->curve()
          ->Duration();
  EXPECT_EQ(duration, player_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
                          ->curve()
                          ->Duration());

  player_->Tick(kInitialTickTime);
  player_->UpdateState(true, nullptr);

  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(initial_value,
            client_.GetScrollOffset(element_id_, ElementListType::ACTIVE));
  EXPECT_EQ(gfx::ScrollOffset(), client_impl_.GetScrollOffset(
                                     element_id_, ElementListType::PENDING));

  player_impl_->Tick(kInitialTickTime);

  EXPECT_TRUE(player_impl_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(initial_value, client_impl_.GetScrollOffset(
                               element_id_, ElementListType::PENDING));

  CreateTestImplLayer(ElementListType::ACTIVE);

  player_impl_->UpdateState(true, events.get());
  DCHECK_EQ(1UL, events->events_.size());

  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);
  player_->Tick(kInitialTickTime + duration / 2);
  player_->UpdateState(true, nullptr);
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(400.f, 150.f),
      client_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime + duration / 2);
  player_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(400.f, 150.f),
      client_impl_.GetScrollOffset(element_id_, ElementListType::PENDING));

  player_impl_->Tick(kInitialTickTime + duration);
  player_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(target_value, client_impl_.GetScrollOffset(
                                        element_id_, ElementListType::PENDING));
  EXPECT_FALSE(player_impl_->keyframe_effect()->HasTickingKeyframeModel());

  player_->Tick(kInitialTickTime + duration);
  player_->UpdateState(true, nullptr);
  EXPECT_VECTOR2DF_EQ(target_value, client_.GetScrollOffset(
                                        element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->HasTickingKeyframeModel());
}

TEST_F(ElementAnimationsTest, ScrollOffsetRemovalClearsScrollDelta) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  // First test the 1-argument version of RemoveKeyframeModel.
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));

  int keyframe_model_id = 1;
  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), keyframe_model_id, 0, TargetProperty::SCROLL_OFFSET));
  keyframe_model->set_needs_synchronized_start_time(true);
  player_->AddKeyframeModel(std::move(keyframe_model));
  PushProperties();
  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(
      player_->keyframe_effect()->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());

  player_->RemoveKeyframeModel(keyframe_model_id);
  EXPECT_TRUE(
      player_->keyframe_effect()->scroll_offset_animation_was_interrupted());

  PushProperties();
  EXPECT_TRUE(player_impl_->keyframe_effect()
                  ->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(
      player_->keyframe_effect()->scroll_offset_animation_was_interrupted());

  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());

  // Now, test the 2-argument version of RemoveKeyframeModel.
  curve = ScrollOffsetAnimationCurve::Create(
      target_value, CubicBezierTimingFunction::CreatePreset(
                        CubicBezierTimingFunction::EaseType::EASE_IN_OUT));
  keyframe_model = KeyframeModel::Create(std::move(curve), keyframe_model_id, 0,
                                         TargetProperty::SCROLL_OFFSET);
  keyframe_model->set_needs_synchronized_start_time(true);
  player_->AddKeyframeModel(std::move(keyframe_model));
  PushProperties();
  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(
      player_->keyframe_effect()->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());

  player_->RemoveKeyframeModel(keyframe_model_id);
  EXPECT_TRUE(
      player_->keyframe_effect()->scroll_offset_animation_was_interrupted());

  PushProperties();
  EXPECT_TRUE(player_impl_->keyframe_effect()
                  ->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(
      player_->keyframe_effect()->scroll_offset_animation_was_interrupted());

  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());

  // Check that removing non-scroll-offset animations does not cause
  // scroll_offset_animation_was_interrupted() to get set.
  keyframe_model_id = AddAnimatedTransformToPlayer(player_.get(), 1.0, 1, 2);
  PushProperties();
  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(
      player_->keyframe_effect()->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());

  player_->RemoveKeyframeModel(keyframe_model_id);
  EXPECT_FALSE(
      player_->keyframe_effect()->scroll_offset_animation_was_interrupted());

  PushProperties();
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(
      player_->keyframe_effect()->scroll_offset_animation_was_interrupted());

  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());

  keyframe_model_id = AddAnimatedFilterToPlayer(player_.get(), 1.0, 0.1f, 0.2f);
  PushProperties();
  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(
      player_->keyframe_effect()->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());

  player_->RemoveKeyframeModel(keyframe_model_id);
  EXPECT_FALSE(
      player_->keyframe_effect()->scroll_offset_animation_was_interrupted());

  PushProperties();
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(
      player_->keyframe_effect()->scroll_offset_animation_was_interrupted());

  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());
}

// Tests that impl-only animations lead to start and finished notifications
// on the impl thread animations's animation delegate.
TEST_F(ElementAnimationsTest,
       NotificationsForImplOnlyAnimationsAreSentToImplThreadDelegate) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  TestAnimationDelegate delegate;
  player_impl_->set_animation_delegate(&delegate);

  gfx::ScrollOffset initial_value(100.f, 300.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
  curve->SetInitialValue(initial_value);
  TimeDelta duration = curve->Duration();
  std::unique_ptr<KeyframeModel> to_add(KeyframeModel::Create(
      std::move(curve), 1, 0, TargetProperty::SCROLL_OFFSET));
  to_add->SetIsImplOnly();
  player_impl_->AddKeyframeModel(std::move(to_add));

  EXPECT_FALSE(delegate.started());
  EXPECT_FALSE(delegate.finished());

  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());

  EXPECT_TRUE(delegate.started());
  EXPECT_FALSE(delegate.finished());

  events = CreateEventsForTesting();
  player_impl_->Tick(kInitialTickTime + duration);
  EXPECT_EQ(duration,
            player_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
                ->curve()
                ->Duration());
  player_impl_->UpdateState(true, events.get());

  EXPECT_TRUE(delegate.started());
  EXPECT_TRUE(delegate.finished());
}

// Tests that specified start times are sent to the main thread delegate
TEST_F(ElementAnimationsTest, SpecifiedStartTimesAreSentToMainThreadDelegate) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  TestAnimationDelegate delegate;
  player_->set_animation_delegate(&delegate);

  int keyframe_model_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0, 1, false);

  const TimeTicks start_time = TicksFromSecondsF(123);
  player_->GetKeyframeModel(TargetProperty::OPACITY)
      ->set_start_time(start_time);

  PushProperties();
  player_impl_->ActivateKeyframeEffects();

  EXPECT_TRUE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());

  auto events = CreateEventsForTesting();
  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());

  // Synchronize the start times.
  EXPECT_EQ(1u, events->events_.size());
  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);

  // Validate start time on the main thread delegate.
  EXPECT_EQ(start_time, delegate.start_time());
}

// Tests animations that are waiting for a synchronized start time do not
// finish.
TEST_F(ElementAnimationsTest,
       AnimationsWaitingForStartTimeDoNotFinishIfTheyOutwaitTheirFinish) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<KeyframeModel> to_add(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  to_add->set_needs_synchronized_start_time(true);

  // We should pause at the first keyframe indefinitely waiting for that
  // animation to start.
  player_->AddKeyframeModel(std::move(to_add));
  player_->Tick(kInitialTickTime);
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Send the synchronized start time.
  player_->keyframe_effect()->NotifyKeyframeModelStarted(AnimationEvent(
      AnimationEvent::STARTED, ElementId(), 1, TargetProperty::OPACITY,
      kInitialTickTime + TimeDelta::FromMilliseconds(2000)));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(5000));
  player_->UpdateState(true, events.get());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->HasTickingKeyframeModel());
}

// Tests that two queued animations affecting the same property run in sequence.
TEST_F(ElementAnimationsTest, TrivialQueuing) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  int animation1_id = 1;
  int animation2_id = 2;
  player_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      animation1_id, 1, TargetProperty::OPACITY));
  player_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      animation2_id, 2, TargetProperty::OPACITY));

  player_->Tick(kInitialTickTime);

  // The first animation should have been started.
  EXPECT_TRUE(player_->keyframe_effect()->GetKeyframeModelById(animation1_id));
  EXPECT_EQ(KeyframeModel::STARTING, player_->keyframe_effect()
                                         ->GetKeyframeModelById(animation1_id)
                                         ->run_state());

  // The second animation still needs to be started.
  EXPECT_TRUE(player_->keyframe_effect()->GetKeyframeModelById(animation2_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            player_->keyframe_effect()
                ->GetKeyframeModelById(animation2_id)
                ->run_state());

  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_->UpdateState(true, events.get());

  // Now the first should be finished, and the second started.
  EXPECT_TRUE(player_->keyframe_effect()->GetKeyframeModelById(animation1_id));
  EXPECT_EQ(KeyframeModel::FINISHED, player_->keyframe_effect()
                                         ->GetKeyframeModelById(animation1_id)
                                         ->run_state());
  EXPECT_TRUE(player_->keyframe_effect()->GetKeyframeModelById(animation2_id));
  EXPECT_EQ(KeyframeModel::RUNNING, player_->keyframe_effect()
                                        ->GetKeyframeModelById(animation2_id)
                                        ->run_state());

  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  player_->UpdateState(true, events.get());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->HasTickingKeyframeModel());
}

// Tests interrupting a transition with another transition.
TEST_F(ElementAnimationsTest, Interrupt) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  player_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  player_->Tick(kInitialTickTime);
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  std::unique_ptr<KeyframeModel> to_add(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));
  player_->AbortKeyframeModels(TargetProperty::OPACITY, false);
  player_->AddKeyframeModel(std::move(to_add));

  // Since the previous animation was aborted, the new animation should start
  // right in this call to animate.
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1500));
  player_->UpdateState(true, events.get());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->HasTickingKeyframeModel());
}

// Tests scheduling two animations to run together when only one property is
// free.
TEST_F(ElementAnimationsTest, ScheduleTogetherWhenAPropertyIsBlocked) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  player_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 1,
      TargetProperty::TRANSFORM));
  player_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 2,
      TargetProperty::TRANSFORM));
  player_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      2, TargetProperty::OPACITY));

  player_->Tick(kInitialTickTime);
  player_->UpdateState(true, events.get());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_->UpdateState(true, events.get());
  // Should not have started the float transition yet.
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  // The float animation should have started at time 1 and should be done.
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  player_->UpdateState(true, events.get());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->HasTickingKeyframeModel());
}

// Tests scheduling two animations to run together with different lengths and
// another animation queued to start when the shorter animation finishes (should
// wait for both to finish).
TEST_F(ElementAnimationsTest, ScheduleTogetherWithAnAnimWaiting) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  player_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(2)), 1,
      TargetProperty::TRANSFORM));
  player_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  player_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));

  // Animations with id 1 should both start now.
  player_->Tick(kInitialTickTime);
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  // The opacity animation should have finished at time 1, but the group
  // of animations with id 1 don't finish until time 2 because of the length
  // of the transform animation.
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  player_->UpdateState(true, events.get());
  // Should not have started the float transition yet.
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // The second opacity animation should start at time 2 and should be done by
  // time 3.
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(3000));
  player_->UpdateState(true, events.get());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->HasTickingKeyframeModel());
}

// Test that a looping animation loops and for the correct number of iterations.
TEST_F(ElementAnimationsTest, TrivialLooping) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<KeyframeModel> to_add(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  to_add->set_iterations(3);
  player_->AddKeyframeModel(std::move(to_add));

  player_->Tick(kInitialTickTime);
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1250));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.25f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1750));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2250));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.25f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2750));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(3000));
  player_->UpdateState(true, events.get());
  EXPECT_FALSE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Just be extra sure.
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(4000));
  player_->UpdateState(true, events.get());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

// Test that an infinitely looping animation does indeed go until aborted.
TEST_F(ElementAnimationsTest, InfiniteLooping) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<KeyframeModel> to_add(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  to_add->set_iterations(-1);
  player_->AddKeyframeModel(std::move(to_add));

  player_->Tick(kInitialTickTime);
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1250));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.25f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1750));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1073741824250));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.25f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1073741824750));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(player_->GetKeyframeModel(TargetProperty::OPACITY));
  player_->GetKeyframeModel(TargetProperty::OPACITY)
      ->SetRunState(KeyframeModel::ABORTED,
                    kInitialTickTime + TimeDelta::FromMilliseconds(750));
  EXPECT_FALSE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

// Test that pausing and resuming work as expected.
TEST_F(ElementAnimationsTest, PauseResume) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  player_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));

  player_->Tick(kInitialTickTime);
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(player_->GetKeyframeModel(TargetProperty::OPACITY));
  player_->GetKeyframeModel(TargetProperty::OPACITY)
      ->SetRunState(KeyframeModel::PAUSED,
                    kInitialTickTime + TimeDelta::FromMilliseconds(500));

  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1024000));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(player_->GetKeyframeModel(TargetProperty::OPACITY));
  player_->GetKeyframeModel(TargetProperty::OPACITY)
      ->SetRunState(KeyframeModel::RUNNING,
                    kInitialTickTime + TimeDelta::FromMilliseconds(1024000));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1024250));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1024500));
  player_->UpdateState(true, events.get());
  EXPECT_FALSE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, AbortAGroupedAnimation) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  const int keyframe_model_id = 2;
  player_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 1, 1,
      TargetProperty::TRANSFORM));
  player_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(2.0, 0.f, 1.f)),
      keyframe_model_id, 1, TargetProperty::OPACITY));
  player_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.75f)),
      3, 2, TargetProperty::OPACITY));

  player_->Tick(kInitialTickTime);
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(
      player_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  player_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->SetRunState(KeyframeModel::ABORTED,
                    kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(!player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, PushUpdatesWhenSynchronizedStartTimeNeeded) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<KeyframeModel> to_add(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(2.0, 0.f, 1.f)),
      0, TargetProperty::OPACITY));
  to_add->set_needs_synchronized_start_time(true);
  player_->AddKeyframeModel(std::move(to_add));

  player_->Tick(kInitialTickTime);
  player_->UpdateState(true, events.get());
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  KeyframeModel* active_keyframe_model =
      player_->GetKeyframeModel(TargetProperty::OPACITY);
  EXPECT_TRUE(active_keyframe_model);
  EXPECT_TRUE(active_keyframe_model->needs_synchronized_start_time());

  EXPECT_TRUE(player_->keyframe_effect()->needs_push_properties());
  PushProperties();
  player_impl_->ActivateKeyframeEffects();

  active_keyframe_model =
      player_impl_->GetKeyframeModel(TargetProperty::OPACITY);
  EXPECT_TRUE(active_keyframe_model);
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            active_keyframe_model->run_state());
}

// Tests that skipping a call to UpdateState works as expected.
TEST_F(ElementAnimationsTest, SkipUpdateState) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<KeyframeModel> first_keyframe_model(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 1,
      TargetProperty::TRANSFORM));
  first_keyframe_model->set_is_controlling_instance_for_test(true);
  player_->AddKeyframeModel(std::move(first_keyframe_model));

  player_->Tick(kInitialTickTime);
  player_->UpdateState(true, events.get());

  std::unique_ptr<KeyframeModel> second_keyframe_model(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      2, TargetProperty::OPACITY));
  second_keyframe_model->set_is_controlling_instance_for_test(true);
  player_->AddKeyframeModel(std::move(second_keyframe_model));

  // Animate but don't UpdateState.
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));

  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  events = CreateEventsForTesting();
  player_->UpdateState(true, events.get());

  // Should have one STARTED event and one FINISHED event.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_NE(events->events_[0].type, events->events_[1].type);

  // The float transition should still be at its starting point.
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(3000));
  player_->UpdateState(true, events.get());

  // The float tranisition should now be done.
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->HasTickingKeyframeModel());
}

// Tests that an animation animations with only a pending observer gets ticked
// but doesn't progress animations past the STARTING state.
TEST_F(ElementAnimationsTest, InactiveObserverGetsTicked) {
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  const int id = 1;
  player_impl_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.5f, 1.f)),
      id, TargetProperty::OPACITY));

  // Without an observer, the animation shouldn't progress to the STARTING
  // state.
  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());
  EXPECT_EQ(0u, events->events_.size());
  EXPECT_EQ(
      KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
      player_impl_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());

  CreateTestImplLayer(ElementListType::PENDING);

  // With only a pending observer, the animation should progress to the
  // STARTING state and get ticked at its starting point, but should not
  // progress to RUNNING.
  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_impl_->UpdateState(true, events.get());
  EXPECT_EQ(0u, events->events_.size());
  EXPECT_EQ(
      KeyframeModel::STARTING,
      player_impl_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));

  // Even when already in the STARTING state, the animation should stay
  // there, and shouldn't be ticked past its starting point.
  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  player_impl_->UpdateState(true, events.get());
  EXPECT_EQ(0u, events->events_.size());
  EXPECT_EQ(
      KeyframeModel::STARTING,
      player_impl_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));

  CreateTestImplLayer(ElementListType::ACTIVE);

  // Now that an active observer has been added, the animation should still
  // initially tick at its starting point, but should now progress to RUNNING.
  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(3000));
  player_impl_->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(
      KeyframeModel::RUNNING,
      player_impl_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // The animation should now tick past its starting point.
  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(3500));
  EXPECT_NE(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_NE(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

// Tests that AbortKeyframeModels aborts all animations targeting the
// specified property.
TEST_F(ElementAnimationsTest, AbortKeyframeModels) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  // Start with several animations, and allow some of them to reach the finished
  // state.
  player_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1.0)), 1, 1,
      TargetProperty::TRANSFORM));
  player_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      2, 2, TargetProperty::OPACITY));
  player_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1.0)), 3, 3,
      TargetProperty::TRANSFORM));
  player_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(2.0)), 4, 4,
      TargetProperty::TRANSFORM));
  player_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      5, 5, TargetProperty::OPACITY));

  player_->Tick(kInitialTickTime);
  player_->UpdateState(true, nullptr);
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_->UpdateState(true, nullptr);

  EXPECT_EQ(KeyframeModel::FINISHED,
            player_->keyframe_effect()->GetKeyframeModelById(1)->run_state());
  EXPECT_EQ(KeyframeModel::FINISHED,
            player_->keyframe_effect()->GetKeyframeModelById(2)->run_state());
  EXPECT_EQ(KeyframeModel::RUNNING,
            player_->keyframe_effect()->GetKeyframeModelById(3)->run_state());
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            player_->keyframe_effect()->GetKeyframeModelById(4)->run_state());
  EXPECT_EQ(KeyframeModel::RUNNING,
            player_->keyframe_effect()->GetKeyframeModelById(5)->run_state());

  player_->AbortKeyframeModels(TargetProperty::TRANSFORM, false);

  // Only un-finished TRANSFORM animations should have been aborted.
  EXPECT_EQ(KeyframeModel::FINISHED,
            player_->keyframe_effect()->GetKeyframeModelById(1)->run_state());
  EXPECT_EQ(KeyframeModel::FINISHED,
            player_->keyframe_effect()->GetKeyframeModelById(2)->run_state());
  EXPECT_EQ(KeyframeModel::ABORTED,
            player_->keyframe_effect()->GetKeyframeModelById(3)->run_state());
  EXPECT_EQ(KeyframeModel::ABORTED,
            player_->keyframe_effect()->GetKeyframeModelById(4)->run_state());
  EXPECT_EQ(KeyframeModel::RUNNING,
            player_->keyframe_effect()->GetKeyframeModelById(5)->run_state());
}

// An animation aborted on the main thread should get deleted on both threads.
TEST_F(ElementAnimationsTest, MainThreadAbortedAnimationGetsDeleted) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  int keyframe_model_id =
      AddOpacityTransitionToPlayer(player_.get(), 1.0, 0.f, 1.f, false);
  EXPECT_TRUE(host_->needs_push_properties());

  PushProperties();

  player_impl_->ActivateKeyframeEffects();

  EXPECT_TRUE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_FALSE(host_->needs_push_properties());

  player_->AbortKeyframeModels(TargetProperty::OPACITY, false);
  EXPECT_EQ(KeyframeModel::ABORTED,
            player_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  EXPECT_TRUE(host_->needs_push_properties());

  player_->Tick(kInitialTickTime);
  player_->UpdateState(true, nullptr);
  EXPECT_EQ(KeyframeModel::ABORTED,
            player_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());

  EXPECT_TRUE(player_->keyframe_effect()->needs_push_properties());
  EXPECT_TRUE(host_->needs_push_properties());

  PushProperties();
  EXPECT_FALSE(player_->keyframe_effect()->needs_push_properties());
  EXPECT_FALSE(host_->needs_push_properties());

  EXPECT_FALSE(
      player_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_FALSE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
}

// An animation aborted on the impl thread should get deleted on both threads.
TEST_F(ElementAnimationsTest, ImplThreadAbortedAnimationGetsDeleted) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  TestAnimationDelegate delegate;
  player_->set_animation_delegate(&delegate);

  int keyframe_model_id =
      AddOpacityTransitionToPlayer(player_.get(), 1.0, 0.f, 1.f, false);

  PushProperties();
  EXPECT_FALSE(host_->needs_push_properties());

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));

  player_impl_->AbortKeyframeModels(TargetProperty::OPACITY, false);
  EXPECT_EQ(
      KeyframeModel::ABORTED,
      player_impl_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  EXPECT_TRUE(host_impl_->needs_push_properties());
  EXPECT_TRUE(player_impl_->keyframe_effect()->needs_push_properties());

  auto events = CreateEventsForTesting();
  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());
  EXPECT_TRUE(host_impl_->needs_push_properties());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::ABORTED, events->events_[0].type);
  EXPECT_EQ(
      KeyframeModel::WAITING_FOR_DELETION,
      player_impl_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());

  player_->keyframe_effect()->NotifyKeyframeModelAborted(events->events_[0]);
  EXPECT_EQ(KeyframeModel::ABORTED,
            player_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  EXPECT_TRUE(delegate.aborted());

  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  player_->UpdateState(true, nullptr);
  EXPECT_TRUE(host_->needs_push_properties());
  EXPECT_EQ(KeyframeModel::WAITING_FOR_DELETION,
            player_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());

  PushProperties();

  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(
      player_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_FALSE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
}

// Test that an impl-only scroll offset animation that needs to be completed on
// the main thread gets deleted.
TEST_F(ElementAnimationsTest, ImplThreadTakeoverAnimationGetsDeleted) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  TestAnimationDelegate delegate_impl;
  player_impl_->set_animation_delegate(&delegate_impl);
  TestAnimationDelegate delegate;
  player_->set_animation_delegate(&delegate);

  // Add impl-only scroll offset animation.
  const int keyframe_model_id = 1;
  gfx::ScrollOffset initial_value(100.f, 300.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
  curve->SetInitialValue(initial_value);
  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), keyframe_model_id, 0, TargetProperty::SCROLL_OFFSET));
  keyframe_model->set_start_time(TicksFromSecondsF(123));
  keyframe_model->SetIsImplOnly();
  player_impl_->AddKeyframeModel(std::move(keyframe_model));

  PushProperties();
  EXPECT_FALSE(host_->needs_push_properties());

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));

  player_impl_->AbortKeyframeModels(TargetProperty::SCROLL_OFFSET, true);
  EXPECT_TRUE(host_impl_->needs_push_properties());
  EXPECT_EQ(KeyframeModel::ABORTED_BUT_NEEDS_COMPLETION,
            player_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
                ->run_state());

  auto events = CreateEventsForTesting();
  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());
  EXPECT_TRUE(delegate_impl.finished());
  EXPECT_TRUE(host_impl_->needs_push_properties());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::TAKEOVER, events->events_[0].type);
  EXPECT_EQ(TicksFromSecondsF(123), events->events_[0].animation_start_time);
  EXPECT_EQ(
      target_value,
      events->events_[0].curve->ToScrollOffsetAnimationCurve()->target_value());
  EXPECT_EQ(nullptr,
            player_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET));

  // MT receives the event to take over.
  player_->keyframe_effect()->NotifyKeyframeModelTakeover(events->events_[0]);
  EXPECT_TRUE(delegate.takeover());

  // SingleKeyframeEffectAnimationPlayer::NotifyAnimationTakeover requests
  // SetNeedsPushProperties to purge CT animations marked for deletion.
  EXPECT_TRUE(player_->keyframe_effect()->needs_push_properties());

  // ElementAnimations::PurgeAnimationsMarkedForDeletion call happens only in
  // ElementAnimations::PushPropertiesTo.
  PushProperties();

  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(
      player_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_FALSE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
}

// Ensure that we only generate FINISHED events for animations in a group
// once all animations in that group are finished.
TEST_F(ElementAnimationsTest, FinishedEventsForGroup) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  const int group_id = 1;

  // Add two animations with the same group id but different durations.
  std::unique_ptr<KeyframeModel> first_keyframe_model(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(2.0)), 1,
      group_id, TargetProperty::TRANSFORM));
  first_keyframe_model->set_is_controlling_instance_for_test(true);
  player_impl_->AddKeyframeModel(std::move(first_keyframe_model));

  std::unique_ptr<KeyframeModel> second_keyframe_model(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      2, group_id, TargetProperty::OPACITY));
  second_keyframe_model->set_is_controlling_instance_for_test(true);
  player_impl_->AddKeyframeModel(std::move(second_keyframe_model));

  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());

  // Both animations should have started.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[1].type);

  events = CreateEventsForTesting();
  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_impl_->UpdateState(true, events.get());

  // The opacity animation should be finished, but should not have generated
  // a FINISHED event yet.
  EXPECT_EQ(0u, events->events_.size());
  EXPECT_EQ(
      KeyframeModel::FINISHED,
      player_impl_->keyframe_effect()->GetKeyframeModelById(2)->run_state());
  EXPECT_EQ(
      KeyframeModel::RUNNING,
      player_impl_->keyframe_effect()->GetKeyframeModelById(1)->run_state());

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  player_impl_->UpdateState(true, events.get());

  // Both animations should have generated FINISHED events.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[0].type);
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[1].type);
}

// Ensure that when a group has a mix of aborted and finished animations,
// we generate a FINISHED event for the finished animation and an ABORTED
// event for the aborted animation.
TEST_F(ElementAnimationsTest, FinishedAndAbortedEventsForGroup) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  // Add two animations with the same group id.
  std::unique_ptr<KeyframeModel> first_keyframe_model(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1.0)), 1,
      TargetProperty::TRANSFORM));
  first_keyframe_model->set_is_controlling_instance_for_test(true);
  player_impl_->AddKeyframeModel(std::move(first_keyframe_model));

  std::unique_ptr<KeyframeModel> second_keyframe_model(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  second_keyframe_model->set_is_controlling_instance_for_test(true);
  player_impl_->AddKeyframeModel(std::move(second_keyframe_model));

  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());

  // Both animations should have started.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[1].type);

  player_impl_->AbortKeyframeModels(TargetProperty::OPACITY, false);

  events = CreateEventsForTesting();
  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_impl_->UpdateState(true, events.get());

  // We should have exactly 2 events: a FINISHED event for the tranform
  // animation, and an ABORTED event for the opacity animation.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[0].type);
  EXPECT_EQ(TargetProperty::TRANSFORM, events->events_[0].target_property);
  EXPECT_EQ(AnimationEvent::ABORTED, events->events_[1].type);
  EXPECT_EQ(TargetProperty::OPACITY, events->events_[1].target_property);
}

TEST_F(ElementAnimationsTest, HasOnlyTranslationTransforms) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  EXPECT_TRUE(player_impl_->keyframe_effect()->HasOnlyTranslationTransforms(
      ElementListType::ACTIVE));
  EXPECT_TRUE(player_impl_->keyframe_effect()->HasOnlyTranslationTransforms(
      ElementListType::PENDING));

  player_impl_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));

  // Opacity animations aren't non-translation transforms.
  EXPECT_TRUE(player_impl_->keyframe_effect()->HasOnlyTranslationTransforms(
      ElementListType::ACTIVE));
  EXPECT_TRUE(player_impl_->keyframe_effect()->HasOnlyTranslationTransforms(
      ElementListType::PENDING));

  std::unique_ptr<KeyframedTransformAnimationCurve> curve1(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1;
  curve1->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  operations1.AppendTranslate(10.0, 15.0, 0.0);
  curve1->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations1, nullptr));

  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve1), 2, 2, TargetProperty::TRANSFORM));
  player_impl_->AddKeyframeModel(std::move(keyframe_model));

  // The only transform animation we've added is a translation.
  EXPECT_TRUE(player_impl_->keyframe_effect()->HasOnlyTranslationTransforms(
      ElementListType::ACTIVE));
  EXPECT_TRUE(player_impl_->keyframe_effect()->HasOnlyTranslationTransforms(
      ElementListType::PENDING));

  std::unique_ptr<KeyframedTransformAnimationCurve> curve2(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations2;
  curve2->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations2, nullptr));
  operations2.AppendScale(2.0, 3.0, 4.0);
  curve2->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations2, nullptr));

  keyframe_model =
      KeyframeModel::Create(std::move(curve2), 3, 3, TargetProperty::TRANSFORM);
  keyframe_model->set_affects_active_elements(false);
  player_impl_->AddKeyframeModel(std::move(keyframe_model));

  // A scale animation is not a translation.
  EXPECT_FALSE(player_impl_->keyframe_effect()->HasOnlyTranslationTransforms(
      ElementListType::PENDING));
  EXPECT_TRUE(player_impl_->keyframe_effect()->HasOnlyTranslationTransforms(
      ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(player_impl_->keyframe_effect()->HasOnlyTranslationTransforms(
      ElementListType::PENDING));
  EXPECT_FALSE(player_impl_->keyframe_effect()->HasOnlyTranslationTransforms(
      ElementListType::ACTIVE));

  player_impl_->keyframe_effect()
      ->GetKeyframeModelById(3)
      ->set_affects_pending_elements(false);
  EXPECT_TRUE(player_impl_->keyframe_effect()->HasOnlyTranslationTransforms(
      ElementListType::PENDING));
  EXPECT_FALSE(player_impl_->keyframe_effect()->HasOnlyTranslationTransforms(
      ElementListType::ACTIVE));

  player_impl_->keyframe_effect()->GetKeyframeModelById(3)->SetRunState(
      KeyframeModel::FINISHED, TicksFromSecondsF(0.0));

  // Only unfinished animations should be considered by
  // HasOnlyTranslationTransforms.
  EXPECT_TRUE(player_impl_->keyframe_effect()->HasOnlyTranslationTransforms(
      ElementListType::PENDING));
  EXPECT_TRUE(player_impl_->keyframe_effect()->HasOnlyTranslationTransforms(
      ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, AnimationStartScale) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  std::unique_ptr<KeyframedTransformAnimationCurve> curve1(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1;
  operations1.AppendScale(2.0, 3.0, 4.0);
  curve1->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  TransformOperations operations2;
  curve1->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations2, nullptr));
  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve1), 1, 1, TargetProperty::TRANSFORM));
  keyframe_model->set_affects_active_elements(false);
  player_impl_->AddKeyframeModel(std::move(keyframe_model));

  float start_scale = 0.f;
  EXPECT_TRUE(player_impl_->keyframe_effect()->AnimationStartScale(
      ElementListType::PENDING, &start_scale));
  EXPECT_EQ(4.f, start_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->AnimationStartScale(
      ElementListType::ACTIVE, &start_scale));
  EXPECT_EQ(0.f, start_scale);

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(player_impl_->keyframe_effect()->AnimationStartScale(
      ElementListType::PENDING, &start_scale));
  EXPECT_EQ(4.f, start_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->AnimationStartScale(
      ElementListType::ACTIVE, &start_scale));
  EXPECT_EQ(4.f, start_scale);

  std::unique_ptr<KeyframedTransformAnimationCurve> curve2(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations3;
  curve2->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations3, nullptr));
  operations3.AppendScale(6.0, 5.0, 4.0);
  curve2->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations3, nullptr));

  player_impl_->RemoveKeyframeModel(1);
  keyframe_model =
      KeyframeModel::Create(std::move(curve2), 2, 2, TargetProperty::TRANSFORM);

  // Reverse Direction
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  keyframe_model->set_affects_active_elements(false);
  player_impl_->AddKeyframeModel(std::move(keyframe_model));

  std::unique_ptr<KeyframedTransformAnimationCurve> curve3(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations4;
  operations4.AppendScale(5.0, 3.0, 1.0);
  curve3->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations4, nullptr));
  TransformOperations operations5;
  curve3->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations5, nullptr));

  keyframe_model =
      KeyframeModel::Create(std::move(curve3), 3, 3, TargetProperty::TRANSFORM);
  keyframe_model->set_affects_active_elements(false);
  player_impl_->AddKeyframeModel(std::move(keyframe_model));

  EXPECT_TRUE(player_impl_->keyframe_effect()->AnimationStartScale(
      ElementListType::PENDING, &start_scale));
  EXPECT_EQ(6.f, start_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->AnimationStartScale(
      ElementListType::ACTIVE, &start_scale));
  EXPECT_EQ(0.f, start_scale);

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(player_impl_->keyframe_effect()->AnimationStartScale(
      ElementListType::PENDING, &start_scale));
  EXPECT_EQ(6.f, start_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->AnimationStartScale(
      ElementListType::ACTIVE, &start_scale));
  EXPECT_EQ(6.f, start_scale);

  player_impl_->keyframe_effect()->GetKeyframeModelById(2)->SetRunState(
      KeyframeModel::FINISHED, TicksFromSecondsF(0.0));

  // Only unfinished animations should be considered by
  // AnimationStartScale.
  EXPECT_TRUE(player_impl_->keyframe_effect()->AnimationStartScale(
      ElementListType::PENDING, &start_scale));
  EXPECT_EQ(5.f, start_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->AnimationStartScale(
      ElementListType::ACTIVE, &start_scale));
  EXPECT_EQ(5.f, start_scale);
}

TEST_F(ElementAnimationsTest, MaximumTargetScale) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  float max_scale = 0.f;
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::PENDING, &max_scale));
  EXPECT_EQ(0.f, max_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(0.f, max_scale);

  std::unique_ptr<KeyframedTransformAnimationCurve> curve1(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1;
  curve1->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  operations1.AppendScale(2.0, 3.0, 4.0);
  curve1->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations1, nullptr));

  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve1), 1, 1, TargetProperty::TRANSFORM));
  keyframe_model->set_affects_active_elements(false);
  player_impl_->AddKeyframeModel(std::move(keyframe_model));

  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::PENDING, &max_scale));
  EXPECT_EQ(4.f, max_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(0.f, max_scale);

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::PENDING, &max_scale));
  EXPECT_EQ(4.f, max_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(4.f, max_scale);

  std::unique_ptr<KeyframedTransformAnimationCurve> curve2(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations2;
  curve2->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations2, nullptr));
  operations2.AppendScale(6.0, 5.0, 4.0);
  curve2->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations2, nullptr));

  keyframe_model =
      KeyframeModel::Create(std::move(curve2), 2, 2, TargetProperty::TRANSFORM);
  keyframe_model->set_affects_active_elements(false);
  player_impl_->AddKeyframeModel(std::move(keyframe_model));

  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::PENDING, &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(4.f, max_scale);

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::PENDING, &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);

  std::unique_ptr<KeyframedTransformAnimationCurve> curve3(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations3;
  curve3->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations3, nullptr));
  operations3.AppendPerspective(6.0);
  curve3->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations3, nullptr));

  keyframe_model =
      KeyframeModel::Create(std::move(curve3), 3, 3, TargetProperty::TRANSFORM);
  keyframe_model->set_affects_active_elements(false);
  player_impl_->AddKeyframeModel(std::move(keyframe_model));

  EXPECT_FALSE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::PENDING, &max_scale));
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);

  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::PENDING, &max_scale));
  EXPECT_FALSE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::ACTIVE, &max_scale));

  player_impl_->keyframe_effect()->GetKeyframeModelById(3)->SetRunState(
      KeyframeModel::FINISHED, TicksFromSecondsF(0.0));
  player_impl_->keyframe_effect()->GetKeyframeModelById(2)->SetRunState(
      KeyframeModel::FINISHED, TicksFromSecondsF(0.0));

  // Only unfinished animations should be considered by
  // MaximumTargetScale.
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::PENDING, &max_scale));
  EXPECT_EQ(4.f, max_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(4.f, max_scale);
}

TEST_F(ElementAnimationsTest, MaximumTargetScaleWithDirection) {
  CreateTestLayer(true, false);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  std::unique_ptr<KeyframedTransformAnimationCurve> curve1(
      KeyframedTransformAnimationCurve::Create());
  TransformOperations operations1;
  operations1.AppendScale(1.0, 2.0, 3.0);
  curve1->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  TransformOperations operations2;
  operations2.AppendScale(4.0, 5.0, 6.0);
  curve1->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations2, nullptr));

  std::unique_ptr<KeyframeModel> keyframe_model_owned(KeyframeModel::Create(
      std::move(curve1), 1, 1, TargetProperty::TRANSFORM));
  KeyframeModel* keyframe_model = keyframe_model_owned.get();
  player_impl_->AddKeyframeModel(std::move(keyframe_model_owned));

  float max_scale = 0.f;

  EXPECT_GT(keyframe_model->playback_rate(), 0.0);

  // NORMAL direction with positive playback rate.
  keyframe_model->set_direction(KeyframeModel::Direction::NORMAL);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::PENDING, &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);

  // ALTERNATE direction with positive playback rate.
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_NORMAL);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::PENDING, &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);

  // REVERSE direction with positive playback rate.
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::PENDING, &max_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(3.f, max_scale);

  // ALTERNATE reverse direction.
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::PENDING, &max_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(3.f, max_scale);

  keyframe_model->set_playback_rate(-1.0);

  // NORMAL direction with negative playback rate.
  keyframe_model->set_direction(KeyframeModel::Direction::NORMAL);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::PENDING, &max_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(3.f, max_scale);

  // ALTERNATE direction with negative playback rate.
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_NORMAL);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::PENDING, &max_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(3.f, max_scale);

  // REVERSE direction with negative playback rate.
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::PENDING, &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);

  // ALTERNATE reverse direction with negative playback rate.
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::PENDING, &max_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_TRUE(player_impl_->keyframe_effect()->MaximumTargetScale(
      ElementListType::ACTIVE, &max_scale));
  EXPECT_EQ(6.f, max_scale);
}

TEST_F(ElementAnimationsTest, NewlyPushedAnimationWaitsForActivation) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  int keyframe_model_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0.5f, 1.f, false);
  EXPECT_TRUE(
      player_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_FALSE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));

  PushProperties();

  EXPECT_TRUE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  EXPECT_TRUE(player_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_pending_elements());
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->GetKeyframeModelById(keyframe_model_id)
                   ->affects_active_elements());

  player_impl_->Tick(kInitialTickTime);
  EXPECT_EQ(KeyframeModel::STARTING,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  player_impl_->UpdateState(true, events.get());

  // Since the animation hasn't been activated, it should still be STARTING
  // rather than RUNNING.
  EXPECT_EQ(KeyframeModel::STARTING,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());

  // Since the animation hasn't been activated, only the pending observer
  // should have been ticked.
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.f, client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(player_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_pending_elements());
  EXPECT_TRUE(player_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_active_elements());

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_impl_->UpdateState(true, events.get());

  // Since the animation has been activated, it should have reached the
  // RUNNING state and the active observer should start to get ticked.
  EXPECT_EQ(KeyframeModel::RUNNING,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ActivationBetweenAnimateAndUpdateState) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  const int keyframe_model_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0.5f, 1.f, true);

  PushProperties();

  EXPECT_TRUE(
      player_impl_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  EXPECT_TRUE(player_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_pending_elements());
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->GetKeyframeModelById(keyframe_model_id)
                   ->affects_active_elements());

  player_impl_->Tick(kInitialTickTime);

  // Since the animation hasn't been activated, only the pending observer
  // should have been ticked.
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.f, client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(player_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_pending_elements());
  EXPECT_TRUE(player_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_active_elements());

  player_impl_->UpdateState(true, events.get());

  // Since the animation has been activated, it should have reached the
  // RUNNING state.
  EXPECT_EQ(KeyframeModel::RUNNING,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));

  // Both elements should have been ticked.
  EXPECT_EQ(0.75f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.75f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ObserverNotifiedWhenTransformAnimationChanges) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  EXPECT_FALSE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 1: An animation that's allowed to run until its finish point.
  AddAnimatedTransformToPlayer(player_.get(), 1.0, 1, 1);
  EXPECT_TRUE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());

  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);
  events->events_.clear();

  // Finish the animation.
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_->UpdateState(true, nullptr);
  EXPECT_FALSE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();

  // Finished animations are pushed, but animations_impl hasn't yet ticked
  // at/past the end of the animation.
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_impl_->UpdateState(true, events.get());
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 2: An animation that's removed before it finishes.
  int keyframe_model_id =
      AddAnimatedTransformToPlayer(player_.get(), 10.0, 2, 2);
  int animation2_id = AddAnimatedTransformToPlayer(player_.get(), 10.0, 2, 1);
  player_->keyframe_effect()
      ->GetKeyframeModelById(animation2_id)
      ->set_time_offset(base::TimeDelta::FromMilliseconds(-10000));
  player_->keyframe_effect()
      ->GetKeyframeModelById(animation2_id)
      ->set_fill_mode(KeyframeModel::FillMode::NONE);
  EXPECT_TRUE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  // animation1 is in effect currently and animation2 isn't. As the element has
  // atleast one animation that's in effect currently, client should be notified
  // that the transform is currently animating.
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  player_impl_->UpdateState(true, events.get());

  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);
  events->events_.clear();

  player_->RemoveKeyframeModel(keyframe_model_id);
  player_->RemoveKeyframeModel(animation2_id);
  EXPECT_FALSE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 3: An animation that's aborted before it finishes.
  keyframe_model_id = AddAnimatedTransformToPlayer(player_.get(), 10.0, 3, 3);
  EXPECT_TRUE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  player_impl_->UpdateState(true, events.get());

  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);
  events->events_.clear();

  player_impl_->AbortKeyframeModels(TargetProperty::TRANSFORM, false);
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(4000));
  player_impl_->UpdateState(true, events.get());

  element_animations_->NotifyAnimationAborted(events->events_[0]);
  EXPECT_FALSE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 4 : An animation that's not in effect.
  keyframe_model_id = AddAnimatedTransformToPlayer(player_.get(), 1.0, 1, 6);
  player_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->set_time_offset(base::TimeDelta::FromMilliseconds(-10000));
  player_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->set_fill_mode(KeyframeModel::FillMode::NONE);

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ObserverNotifiedWhenOpacityAnimationChanges) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  EXPECT_FALSE(client_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 1: An animation that's allowed to run until its finish point.
  AddOpacityTransitionToPlayer(player_.get(), 1.0, 0.f, 1.f,
                               false /*use_timing_function*/);
  EXPECT_TRUE(client_.GetHasPotentialOpacityAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());

  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);
  events->events_.clear();

  // Finish the animation.
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_->UpdateState(true, nullptr);
  EXPECT_FALSE(client_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                      ElementListType::ACTIVE));

  PushProperties();

  // Finished animations are pushed, but animations_impl hasn't yet ticked
  // at/past the end of the animation.
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_impl_->UpdateState(true, events.get());
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 2: An animation that's removed before it finishes.
  int keyframe_model_id = AddOpacityTransitionToPlayer(
      player_.get(), 10.0, 0.f, 1.f, false /*use_timing_function*/);
  EXPECT_TRUE(client_.GetHasPotentialOpacityAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  player_impl_->UpdateState(true, events.get());

  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);
  events->events_.clear();

  player_->RemoveKeyframeModel(keyframe_model_id);
  EXPECT_FALSE(client_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                      ElementListType::ACTIVE));

  PushProperties();
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 3: An animation that's aborted before it finishes.
  keyframe_model_id = AddOpacityTransitionToPlayer(
      player_.get(), 10.0, 0.f, 0.5f, false /*use_timing_function*/);
  EXPECT_TRUE(client_.GetHasPotentialOpacityAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  player_impl_->UpdateState(true, events.get());

  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);
  events->events_.clear();

  player_impl_->AbortKeyframeModels(TargetProperty::OPACITY, false);
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(4000));
  player_impl_->UpdateState(true, events.get());

  element_animations_->NotifyAnimationAborted(events->events_[0]);
  EXPECT_FALSE(client_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                      ElementListType::ACTIVE));

  // Case 4 : An animation that's not in effect.
  keyframe_model_id = AddOpacityTransitionToPlayer(
      player_.get(), 1.0, 0.f, 0.5f, false /*use_timing_function*/);
  player_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->set_time_offset(base::TimeDelta::FromMilliseconds(-10000));
  player_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->set_fill_mode(KeyframeModel::FillMode::NONE);

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ObserverNotifiedWhenFilterAnimationChanges) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  EXPECT_FALSE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 1: An animation that's allowed to run until its finish point.
  AddAnimatedFilterToPlayer(player_.get(), 1.0, 0.f, 1.f);
  EXPECT_TRUE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                     ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                    ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());

  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);
  events->events_.clear();

  // Finish the animation.
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_->UpdateState(true, nullptr);
  EXPECT_FALSE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  PushProperties();

  // Finished animations are pushed, but animations_impl hasn't yet ticked
  // at/past the end of the animation.
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_impl_->UpdateState(true, events.get());
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 2: An animation that's removed before it finishes.
  int keyframe_model_id =
      AddAnimatedFilterToPlayer(player_.get(), 10.0, 0.f, 1.f);
  EXPECT_TRUE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                     ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                    ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  player_impl_->UpdateState(true, events.get());

  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);
  events->events_.clear();

  player_->RemoveKeyframeModel(keyframe_model_id);
  EXPECT_FALSE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  PushProperties();
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 3: An animation that's aborted before it finishes.
  keyframe_model_id = AddAnimatedFilterToPlayer(player_.get(), 10.0, 0.f, 0.5f);
  EXPECT_TRUE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                     ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                    ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  player_impl_->UpdateState(true, events.get());

  player_->keyframe_effect()->NotifyKeyframeModelStarted(events->events_[0]);
  events->events_.clear();

  player_impl_->AbortKeyframeModels(TargetProperty::FILTER, false);
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(4000));
  player_impl_->UpdateState(true, events.get());

  element_animations_->NotifyAnimationAborted(events->events_[0]);
  EXPECT_FALSE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  // Case 4 : An animation that's not in effect.
  keyframe_model_id = AddAnimatedFilterToPlayer(player_.get(), 1.0, 0.f, 0.5f);
  player_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->set_time_offset(base::TimeDelta::FromMilliseconds(-10000));
  player_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->set_fill_mode(KeyframeModel::FillMode::NONE);

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ClippedOpacityValues) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  AddOpacityTransitionToPlayer(player_.get(), 1, 1.f, 2.f, true);

  player_->Tick(kInitialTickTime);
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Opacity values are clipped [0,1]
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ClippedNegativeOpacityValues) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  AddOpacityTransitionToPlayer(player_.get(), 1, 0.f, -2.f, true);

  player_->Tick(kInitialTickTime);
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Opacity values are clipped [0,1]
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, PushedDeletedAnimationWaitsForActivation) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  const int keyframe_model_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0.5f, 1.f, true);
  PushProperties();
  player_impl_->ActivateKeyframeEffects();
  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());
  EXPECT_EQ(KeyframeModel::RUNNING,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(player_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_pending_elements());
  EXPECT_TRUE(player_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_active_elements());

  // Delete the animation on the main-thread animations.
  player_->RemoveKeyframeModel(
      player_->GetKeyframeModel(TargetProperty::OPACITY)->id());
  PushProperties();

  // The animation should no longer affect pending elements.
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->GetKeyframeModelById(keyframe_model_id)
                   ->affects_pending_elements());
  EXPECT_TRUE(player_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_active_elements());

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  player_impl_->UpdateState(true, events.get());

  // Only the active observer should have been ticked.
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.75f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();
  events = CreateEventsForTesting();
  player_impl_->UpdateState(true, events.get());

  // After Activation the animation doesn't affect neither active nor pending
  // thread. UpdateState for this animation would put the animation to wait for
  // deletion state.
  EXPECT_EQ(KeyframeModel::WAITING_FOR_DELETION,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  EXPECT_EQ(1u, events->events_.size());

  // The animation is finished on impl thread, and main thread will delete it
  // during commit.
  player_->animation_host()->SetAnimationEvents(std::move(events));
  PushProperties();
  EXPECT_FALSE(player_impl_->keyframe_effect()->has_any_keyframe_model());
}

// Tests that an animation that affects only active elements won't block
// an animation that affects only pending elements from starting.
TEST_F(ElementAnimationsTest, StartAnimationsAffectingDifferentObservers) {
  CreateTestLayer(true, true);
  AttachTimelinePlayerLayer();
  CreateImplTimelineAndPlayer();

  auto events = CreateEventsForTesting();

  const int first_keyframe_model_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 0.f, 1.f, true);

  PushProperties();
  player_impl_->ActivateKeyframeEffects();
  player_impl_->Tick(kInitialTickTime);
  player_impl_->UpdateState(true, events.get());

  // Remove the first animation from the main-thread animations, and add a
  // new animation affecting the same property.
  player_->RemoveKeyframeModel(
      player_->GetKeyframeModel(TargetProperty::OPACITY)->id());
  const int second_keyframe_model_id =
      AddOpacityTransitionToPlayer(player_.get(), 1, 1.f, 0.5f, true);
  PushProperties();

  // The original animation should only affect active elements, and the new
  // animation should only affect pending elements.
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->GetKeyframeModelById(first_keyframe_model_id)
                   ->affects_pending_elements());
  EXPECT_TRUE(player_impl_->keyframe_effect()
                  ->GetKeyframeModelById(first_keyframe_model_id)
                  ->affects_active_elements());
  EXPECT_TRUE(player_impl_->keyframe_effect()
                  ->GetKeyframeModelById(second_keyframe_model_id)
                  ->affects_pending_elements());
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->GetKeyframeModelById(second_keyframe_model_id)
                   ->affects_active_elements());

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  player_impl_->UpdateState(true, events.get());

  // The original animation should still be running, and the new animation
  // should be starting.
  EXPECT_EQ(KeyframeModel::RUNNING,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(first_keyframe_model_id)
                ->run_state());
  EXPECT_EQ(KeyframeModel::STARTING,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(second_keyframe_model_id)
                ->run_state());

  // The active observer should have been ticked by the original animation,
  // and the pending observer should have been ticked by the new animation.
  EXPECT_EQ(1.f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  player_impl_->ActivateKeyframeEffects();

  // The original animation no longer affect either elements, and the new
  // animation should now affect both elements.
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->GetKeyframeModelById(first_keyframe_model_id)
                   ->affects_pending_elements());
  EXPECT_FALSE(player_impl_->keyframe_effect()
                   ->GetKeyframeModelById(first_keyframe_model_id)
                   ->affects_active_elements());
  EXPECT_TRUE(player_impl_->keyframe_effect()
                  ->GetKeyframeModelById(second_keyframe_model_id)
                  ->affects_pending_elements());
  EXPECT_TRUE(player_impl_->keyframe_effect()
                  ->GetKeyframeModelById(second_keyframe_model_id)
                  ->affects_active_elements());

  player_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  player_impl_->UpdateState(true, events.get());

  // The original animation should be marked for waiting for deletion.
  EXPECT_EQ(KeyframeModel::WAITING_FOR_DELETION,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(first_keyframe_model_id)
                ->run_state());

  // The new animation should be running, and the active observer should have
  // been ticked at the new animation's starting point.
  EXPECT_EQ(KeyframeModel::RUNNING,
            player_impl_->keyframe_effect()
                ->GetKeyframeModelById(second_keyframe_model_id)
                ->run_state());
  EXPECT_EQ(1.f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(1.f, client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, TestIsCurrentlyAnimatingProperty) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  // Create an animation that initially affects only pending elements.
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  keyframe_model->set_affects_active_elements(false);

  player_->AddKeyframeModel(std::move(keyframe_model));
  player_->Tick(kInitialTickTime);
  EXPECT_TRUE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  player_->UpdateState(true, nullptr);
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());

  EXPECT_TRUE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  player_->ActivateKeyframeEffects();

  EXPECT_TRUE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(10));
  player_->UpdateState(true, nullptr);

  EXPECT_TRUE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Tick past the end of the animation.
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1100));
  player_->UpdateState(true, nullptr);

  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, TestIsAnimatingPropertyTimeOffsetFillMode) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  // Create an animation that initially affects only pending elements, and has
  // a start delay of 2 seconds.
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
  keyframe_model->set_time_offset(TimeDelta::FromMilliseconds(-2000));
  keyframe_model->set_affects_active_elements(false);

  player_->AddKeyframeModel(std::move(keyframe_model));

  player_->Tick(kInitialTickTime);

  // Since the animation has a start delay, the elements it affects have a
  // potentially running transform animation but aren't currently animating
  // transform.
  EXPECT_TRUE(player_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(player_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_FALSE(player_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(player_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  player_->ActivateKeyframeEffects();

  EXPECT_TRUE(player_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(player_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_TRUE(player_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_FALSE(player_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(player_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  player_->UpdateState(true, nullptr);

  // Tick past the start delay.
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  player_->UpdateState(true, nullptr);
  EXPECT_TRUE(player_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(player_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_TRUE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));

  // After the animaton finishes, the elements it affects have neither a
  // potentially running transform animation nor a currently running transform
  // animation.
  player_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(4000));
  player_->UpdateState(true, nullptr);
  EXPECT_FALSE(player_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(player_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(player_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, DestroyTestMainLayerBeforePushProperties) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();
  EXPECT_EQ(0u, host_->ticking_players_for_testing().size());

  player_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));
  EXPECT_EQ(1u, host_->ticking_players_for_testing().size());

  DestroyTestMainLayer();
  EXPECT_EQ(0u, host_->ticking_players_for_testing().size());

  PushProperties();
  EXPECT_EQ(0u, host_->ticking_players_for_testing().size());
  EXPECT_EQ(0u, host_impl_->ticking_players_for_testing().size());
}

TEST_F(ElementAnimationsTest, RemoveAndReAddPlayerToTicking) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();
  EXPECT_EQ(0u, host_->ticking_players_for_testing().size());

  // Add an animation and ensure the player is in the host's ticking players.
  // Remove the player using RemoveFromTicking().
  player_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));
  ASSERT_EQ(1u, host_->ticking_players_for_testing().size());
  player_->keyframe_effect()->RemoveFromTicking();
  ASSERT_EQ(0u, host_->ticking_players_for_testing().size());

  // Ensure that adding a new animation will correctly update the ticking
  // players list.
  player_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));
  EXPECT_EQ(1u, host_->ticking_players_for_testing().size());
}

TEST_F(ElementAnimationsTest, TickingKeyframeModelsCount) {
  CreateTestLayer(false, false);
  AttachTimelinePlayerLayer();

  // Add an animation and ensure the player is in the host's ticking players.
  player_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));
  EXPECT_EQ(1u, player_->TickingKeyframeModelsCount());
  EXPECT_EQ(1u, host_->CompositedAnimationsCount());
  player_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 1,
      TargetProperty::TRANSFORM));
  EXPECT_EQ(2u, player_->TickingKeyframeModelsCount());
  EXPECT_EQ(2u, host_->CompositedAnimationsCount());
  player_->keyframe_effect()->RemoveFromTicking();
  EXPECT_EQ(0u, host_->CompositedAnimationsCount());
}

}  // namespace
}  // namespace cc
