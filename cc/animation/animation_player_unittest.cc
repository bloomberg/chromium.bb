// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_player.h"

#include "base/strings/stringprintf.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_ticker.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/animation_timelines_test_common.h"

namespace cc {
namespace {

class AnimationPlayerTest : public AnimationTimelinesTest {
 public:
  AnimationPlayerTest()
      : player_(AnimationPlayer::Create(player_id_)),
        group_id_(100),
        animation_id_(100) {
    ticker_id_ = player_->NextTickerId();
  }
  ~AnimationPlayerTest() override = default;

  int NextGroupId() { return ++group_id_; }

  int NextAnimationId() { return ++animation_id_; }

  int AddOpacityTransition(AnimationPlayer* target,
                           double duration,
                           float start_opacity,
                           float end_opacity,
                           bool use_timing_function,
                           TickerId ticker_id) {
    std::unique_ptr<KeyframedFloatAnimationCurve> curve(
        KeyframedFloatAnimationCurve::Create());

    std::unique_ptr<TimingFunction> func;
    if (!use_timing_function)
      func = CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE);
    if (duration > 0.0)
      curve->AddKeyframe(FloatKeyframe::Create(base::TimeDelta(), start_opacity,
                                               std::move(func)));
    curve->AddKeyframe(FloatKeyframe::Create(
        base::TimeDelta::FromSecondsD(duration), end_opacity, nullptr));

    int id = NextAnimationId();

    std::unique_ptr<Animation> animation(Animation::Create(
        std::move(curve), id, NextGroupId(), TargetProperty::OPACITY));
    animation->set_needs_synchronized_start_time(true);

    target->AddAnimationForTicker(std::move(animation), ticker_id);
    return id;
  }

  int AddAnimatedTransform(AnimationPlayer* target,
                           double duration,
                           TransformOperations start_operations,
                           TransformOperations operations,
                           TickerId ticker_id) {
    std::unique_ptr<KeyframedTransformAnimationCurve> curve(
        KeyframedTransformAnimationCurve::Create());

    if (duration > 0.0) {
      curve->AddKeyframe(TransformKeyframe::Create(base::TimeDelta(),
                                                   start_operations, nullptr));
    }

    curve->AddKeyframe(TransformKeyframe::Create(
        base::TimeDelta::FromSecondsD(duration), operations, nullptr));

    int id = NextAnimationId();

    std::unique_ptr<Animation> animation(Animation::Create(
        std::move(curve), id, NextGroupId(), TargetProperty::TRANSFORM));
    animation->set_needs_synchronized_start_time(true);

    target->AddAnimationForTicker(std::move(animation), ticker_id);
    return id;
  }

  int AddAnimatedTransform(AnimationPlayer* target,
                           double duration,
                           int delta_x,
                           int delta_y,
                           TickerId ticker_id) {
    TransformOperations start_operations;
    if (duration > 0.0) {
      start_operations.AppendTranslate(0, 0, 0.0);
    }

    TransformOperations operations;
    operations.AppendTranslate(delta_x, delta_y, 0.0);
    return AddAnimatedTransform(target, duration, start_operations, operations,
                                ticker_id);
  }

  int AddAnimatedFilter(AnimationPlayer* target,
                        double duration,
                        float start_brightness,
                        float end_brightness,
                        TickerId ticker_id) {
    std::unique_ptr<KeyframedFilterAnimationCurve> curve(
        KeyframedFilterAnimationCurve::Create());

    if (duration > 0.0) {
      FilterOperations start_filters;
      start_filters.Append(
          FilterOperation::CreateBrightnessFilter(start_brightness));
      curve->AddKeyframe(
          FilterKeyframe::Create(base::TimeDelta(), start_filters, nullptr));
    }

    FilterOperations filters;
    filters.Append(FilterOperation::CreateBrightnessFilter(end_brightness));
    curve->AddKeyframe(FilterKeyframe::Create(
        base::TimeDelta::FromSecondsD(duration), filters, nullptr));

    int id = NextAnimationId();

    std::unique_ptr<Animation> animation(Animation::Create(
        std::move(curve), id, NextGroupId(), TargetProperty::FILTER));
    animation->set_needs_synchronized_start_time(true);

    target->AddAnimationForTicker(std::move(animation), ticker_id);
    return id;
  }

  void CheckTickerAndTimelineNeedsPushProperties(bool needs_push_properties,
                                                 TickerId ticker_id) const {
    AnimationTicker* ticker = player_->GetTickerById(ticker_id);
    EXPECT_EQ(ticker->needs_push_properties(), needs_push_properties);
    EXPECT_EQ(timeline_->needs_push_properties(), needs_push_properties);
  }

 protected:
  scoped_refptr<AnimationPlayer> player_;
  scoped_refptr<AnimationPlayer> player_impl_;
  int group_id_;
  TickerId ticker_id_;
  int animation_id_;
};
// See element_animations_unittest.cc for active/pending observers tests.

TEST_F(AnimationPlayerTest, AttachDetachLayerIfTimelineAttached) {
  player_->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id_));
  ASSERT_TRUE(player_->GetTickerById(ticker_id_));
  EXPECT_FALSE(player_->GetTickerById(ticker_id_)->needs_push_properties());
  CheckTickerAndTimelineNeedsPushProperties(false, ticker_id_);

  host_->AddAnimationTimeline(timeline_);
  EXPECT_TRUE(timeline_->needs_push_properties());

  timeline_->AttachPlayer(player_);
  EXPECT_FALSE(player_->element_animations(ticker_id_));
  EXPECT_FALSE(player_->element_id_of_ticker(ticker_id_));
  EXPECT_TRUE(timeline_->needs_push_properties());
  EXPECT_FALSE(player_->GetTickerById(ticker_id_)->needs_push_properties());

  host_->PushPropertiesTo(host_impl_);

  EXPECT_FALSE(GetImplTickerForLayerId(element_id_));

  timeline_impl_ = host_impl_->GetTimelineById(timeline_id_);
  EXPECT_TRUE(timeline_impl_);
  player_impl_ = timeline_impl_->GetPlayerById(player_id_);
  EXPECT_TRUE(player_impl_);

  EXPECT_FALSE(player_impl_->element_animations(ticker_id_));
  EXPECT_FALSE(player_impl_->element_id_of_ticker(ticker_id_));
  EXPECT_FALSE(player_->GetTickerById(ticker_id_)->needs_push_properties());
  EXPECT_FALSE(timeline_->needs_push_properties());

  player_->AttachElementForTicker(element_id_, ticker_id_);
  EXPECT_EQ(player_->GetTickerById(ticker_id_),
            GetTickerForElementId(element_id_));
  EXPECT_TRUE(player_->element_animations(ticker_id_));
  EXPECT_EQ(player_->element_id_of_ticker(ticker_id_), element_id_);
  CheckTickerAndTimelineNeedsPushProperties(true, ticker_id_);

  host_->PushPropertiesTo(host_impl_);

  EXPECT_EQ(player_impl_->GetTickerById(ticker_id_),
            GetImplTickerForLayerId(element_id_));
  EXPECT_TRUE(player_impl_->element_animations(ticker_id_));
  EXPECT_EQ(player_impl_->element_id_of_ticker(ticker_id_), element_id_);
  CheckTickerAndTimelineNeedsPushProperties(false, ticker_id_);

  player_->DetachElement();
  EXPECT_FALSE(GetTickerForElementId(element_id_));
  EXPECT_FALSE(player_->element_animations(ticker_id_));
  EXPECT_FALSE(player_->element_id_of_ticker(ticker_id_));
  CheckTickerAndTimelineNeedsPushProperties(true, ticker_id_);

  host_->PushPropertiesTo(host_impl_);

  EXPECT_FALSE(GetImplTickerForLayerId(element_id_));
  EXPECT_FALSE(player_impl_->element_animations(ticker_id_));
  EXPECT_FALSE(player_impl_->element_id_of_ticker(ticker_id_));
  CheckTickerAndTimelineNeedsPushProperties(false, ticker_id_);

  timeline_->DetachPlayer(player_);
  EXPECT_FALSE(player_->animation_timeline());
  EXPECT_FALSE(player_->element_animations(ticker_id_));
  EXPECT_FALSE(player_->element_id_of_ticker(ticker_id_));
  EXPECT_TRUE(timeline_->needs_push_properties());
  EXPECT_FALSE(player_->GetTickerById(ticker_id_)->needs_push_properties());
  host_->PushPropertiesTo(host_impl_);
  CheckTickerAndTimelineNeedsPushProperties(false, ticker_id_);
}

TEST_F(AnimationPlayerTest, AttachDetachTimelineIfLayerAttached) {
  host_->AddAnimationTimeline(timeline_);

  player_->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id_));
  ASSERT_TRUE(player_->GetTickerById(ticker_id_));

  EXPECT_FALSE(player_->GetTickerById(ticker_id_)->element_animations());
  EXPECT_FALSE(player_->GetTickerById(ticker_id_)->element_id());
  EXPECT_FALSE(player_->GetTickerById(ticker_id_)->needs_push_properties());

  player_->AttachElementForTicker(element_id_, ticker_id_);
  EXPECT_FALSE(player_->animation_timeline());
  EXPECT_FALSE(GetTickerForElementId(element_id_));
  EXPECT_FALSE(player_->GetTickerById(ticker_id_)->element_animations());
  EXPECT_EQ(player_->GetTickerById(ticker_id_)->element_id(), element_id_);
  EXPECT_FALSE(player_->GetTickerById(ticker_id_)->needs_push_properties());

  timeline_->AttachPlayer(player_);
  EXPECT_EQ(timeline_, player_->animation_timeline());
  EXPECT_EQ(player_->GetTickerById(ticker_id_),
            GetTickerForElementId(element_id_));
  EXPECT_TRUE(player_->GetTickerById(ticker_id_)->element_animations());
  EXPECT_EQ(player_->GetTickerById(ticker_id_)->element_id(), element_id_);
  EXPECT_TRUE(player_->GetTickerById(ticker_id_)->needs_push_properties());

  // Removing player from timeline detaches layer.
  timeline_->DetachPlayer(player_);
  EXPECT_FALSE(player_->animation_timeline());
  EXPECT_FALSE(GetTickerForElementId(element_id_));
  EXPECT_FALSE(player_->GetTickerById(ticker_id_)->element_animations());
  EXPECT_FALSE(player_->GetTickerById(ticker_id_)->element_id());
  EXPECT_TRUE(player_->GetTickerById(ticker_id_)->needs_push_properties());
}

TEST_F(AnimationPlayerTest, PropertiesMutate) {
  client_.RegisterElement(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElement(element_id_, ElementListType::PENDING);
  client_impl_.RegisterElement(element_id_, ElementListType::ACTIVE);

  host_->AddAnimationTimeline(timeline_);

  player_->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id_));
  ASSERT_TRUE(player_->GetTickerById(ticker_id_));

  timeline_->AttachPlayer(player_);
  player_->AttachElementForTicker(element_id_, ticker_id_);
  CheckTickerAndTimelineNeedsPushProperties(true, ticker_id_);

  host_->PushPropertiesTo(host_impl_);
  CheckTickerAndTimelineNeedsPushProperties(false, ticker_id_);

  const float start_opacity = .7f;
  const float end_opacity = .3f;

  const float start_brightness = .6f;
  const float end_brightness = .4f;

  const int transform_x = 10;
  const int transform_y = 20;

  const double duration = 1.;

  AddOpacityTransition(player_.get(), duration, start_opacity, end_opacity,
                       false, ticker_id_);

  AddAnimatedTransform(player_.get(), duration, transform_x, transform_y,
                       ticker_id_);
  AddAnimatedFilter(player_.get(), duration, start_brightness, end_brightness,
                    ticker_id_);
  CheckTickerAndTimelineNeedsPushProperties(true, ticker_id_);

  host_->PushPropertiesTo(host_impl_);
  CheckTickerAndTimelineNeedsPushProperties(false, ticker_id_);

  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         TargetProperty::OPACITY));
  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         TargetProperty::TRANSFORM));
  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         TargetProperty::FILTER));

  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, ElementListType::ACTIVE, TargetProperty::OPACITY));
  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, ElementListType::ACTIVE, TargetProperty::TRANSFORM));
  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, ElementListType::ACTIVE, TargetProperty::FILTER));

  host_impl_->ActivateAnimations();

  base::TimeTicks time;
  time += base::TimeDelta::FromSecondsD(0.1);
  TickAnimationsTransferEvents(time, 3u);
  CheckTickerAndTimelineNeedsPushProperties(false, ticker_id_);

  time += base::TimeDelta::FromSecondsD(duration);
  TickAnimationsTransferEvents(time, 3u);
  CheckTickerAndTimelineNeedsPushProperties(true, ticker_id_);

  client_.ExpectOpacityPropertyMutated(element_id_, ElementListType::ACTIVE,
                                       end_opacity);
  client_.ExpectTransformPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         transform_x, transform_y);
  client_.ExpectFilterPropertyMutated(element_id_, ElementListType::ACTIVE,
                                      end_brightness);

  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::ACTIVE, end_opacity);
  client_impl_.ExpectTransformPropertyMutated(
      element_id_, ElementListType::ACTIVE, transform_x, transform_y);
  client_impl_.ExpectFilterPropertyMutated(element_id_, ElementListType::ACTIVE,
                                           end_brightness);

  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::PENDING, end_opacity);
  client_impl_.ExpectTransformPropertyMutated(
      element_id_, ElementListType::PENDING, transform_x, transform_y);
  client_impl_.ExpectFilterPropertyMutated(
      element_id_, ElementListType::PENDING, end_brightness);
}

TEST_F(AnimationPlayerTest, AttachTwoPlayersToOneLayer) {
  TestAnimationDelegate delegate1;
  TestAnimationDelegate delegate2;

  client_.RegisterElement(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElement(element_id_, ElementListType::PENDING);
  client_impl_.RegisterElement(element_id_, ElementListType::ACTIVE);

  scoped_refptr<AnimationPlayer> player1 = AnimationPlayer::Create(100);
  scoped_refptr<AnimationPlayer> player2 = AnimationPlayer::Create(200);

  TickerId ticker_id1 = player1->NextTickerId();
  player1->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id1));
  ASSERT_TRUE(player1->GetTickerById(ticker_id1));
  TickerId ticker_id2 = player2->NextTickerId();
  player2->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id2));
  ASSERT_TRUE(player2->GetTickerById(ticker_id2));

  host_->AddAnimationTimeline(timeline_);

  timeline_->AttachPlayer(player1);
  EXPECT_TRUE(timeline_->needs_push_properties());

  timeline_->AttachPlayer(player2);
  EXPECT_TRUE(timeline_->needs_push_properties());

  player1->set_animation_delegate(&delegate1);
  player2->set_animation_delegate(&delegate2);

  // Attach players to the same layer.
  player1->AttachElementForTicker(element_id_, ticker_id1);
  player2->AttachElementForTicker(element_id_, ticker_id2);

  const float start_opacity = .7f;
  const float end_opacity = .3f;

  const int transform_x = 10;
  const int transform_y = 20;

  const double duration = 1.;

  AddOpacityTransition(player1.get(), duration, start_opacity, end_opacity,
                       false, ticker_id1);
  AddAnimatedTransform(player2.get(), duration, transform_x, transform_y,
                       ticker_id2);

  host_->PushPropertiesTo(host_impl_);
  host_impl_->ActivateAnimations();

  EXPECT_FALSE(delegate1.started());
  EXPECT_FALSE(delegate1.finished());

  EXPECT_FALSE(delegate2.started());
  EXPECT_FALSE(delegate2.finished());

  base::TimeTicks time;
  time += base::TimeDelta::FromSecondsD(0.1);
  TickAnimationsTransferEvents(time, 2u);

  EXPECT_TRUE(delegate1.started());
  EXPECT_FALSE(delegate1.finished());

  EXPECT_TRUE(delegate2.started());
  EXPECT_FALSE(delegate2.finished());

  EXPECT_FALSE(player1->GetTickerById(ticker_id1)->needs_push_properties());
  EXPECT_FALSE(player2->GetTickerById(ticker_id2)->needs_push_properties());

  time += base::TimeDelta::FromSecondsD(duration);
  TickAnimationsTransferEvents(time, 2u);

  EXPECT_TRUE(delegate1.finished());
  EXPECT_TRUE(delegate2.finished());

  EXPECT_TRUE(player1->GetTickerById(ticker_id1)->needs_push_properties());
  EXPECT_TRUE(player2->GetTickerById(ticker_id2)->needs_push_properties());

  client_.ExpectOpacityPropertyMutated(element_id_, ElementListType::ACTIVE,
                                       end_opacity);
  client_.ExpectTransformPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         transform_x, transform_y);

  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::ACTIVE, end_opacity);
  client_impl_.ExpectTransformPropertyMutated(
      element_id_, ElementListType::ACTIVE, transform_x, transform_y);

  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::PENDING, end_opacity);
  client_impl_.ExpectTransformPropertyMutated(
      element_id_, ElementListType::PENDING, transform_x, transform_y);
}

TEST_F(AnimationPlayerTest, AddRemoveAnimationToNonAttachedPlayer) {
  client_.RegisterElement(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElement(element_id_, ElementListType::PENDING);
  client_impl_.RegisterElement(element_id_, ElementListType::ACTIVE);

  player_->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id_));
  ASSERT_TRUE(player_->GetTickerById(ticker_id_));

  const double duration = 1.;
  const float start_opacity = .7f;
  const float end_opacity = .3f;

  const int filter_id =
      AddAnimatedFilter(player_.get(), duration, 0.1f, 0.9f, ticker_id_);
  AddOpacityTransition(player_.get(), duration, start_opacity, end_opacity,
                       false, ticker_id_);

  EXPECT_FALSE(player_->GetTickerById(ticker_id_)->needs_push_properties());

  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(player_);

  EXPECT_FALSE(player_->GetTickerById(ticker_id_)->needs_push_properties());
  EXPECT_FALSE(player_->GetTickerById(ticker_id_)->element_animations());
  player_->RemoveAnimationForTicker(filter_id, ticker_id_);
  EXPECT_FALSE(player_->GetTickerById(ticker_id_)->needs_push_properties());

  player_->AttachElementForTicker(element_id_, ticker_id_);

  EXPECT_TRUE(player_->GetTickerById(ticker_id_)->element_animations());
  EXPECT_FALSE(player_->GetTickerById(ticker_id_)
                   ->element_animations()
                   ->HasAnyAnimationTargetingProperty(TargetProperty::FILTER));
  EXPECT_TRUE(player_->GetTickerById(ticker_id_)
                  ->element_animations()
                  ->HasAnyAnimationTargetingProperty(TargetProperty::OPACITY));
  EXPECT_TRUE(player_->GetTickerById(ticker_id_)->needs_push_properties());

  host_->PushPropertiesTo(host_impl_);

  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         TargetProperty::OPACITY));
  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, ElementListType::ACTIVE, TargetProperty::OPACITY));

  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         TargetProperty::FILTER));
  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, ElementListType::ACTIVE, TargetProperty::FILTER));

  host_impl_->ActivateAnimations();

  base::TimeTicks time;
  time += base::TimeDelta::FromSecondsD(0.1);
  TickAnimationsTransferEvents(time, 1u);

  time += base::TimeDelta::FromSecondsD(duration);
  TickAnimationsTransferEvents(time, 1u);

  client_.ExpectOpacityPropertyMutated(element_id_, ElementListType::ACTIVE,
                                       end_opacity);
  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::ACTIVE, end_opacity);
  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::PENDING, end_opacity);

  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         TargetProperty::FILTER));
  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, ElementListType::ACTIVE, TargetProperty::FILTER));
}

TEST_F(AnimationPlayerTest, AddRemoveAnimationCausesSetNeedsCommit) {
  client_.RegisterElement(element_id_, ElementListType::ACTIVE);
  host_->AddAnimationTimeline(timeline_);
  player_->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id_));
  ASSERT_TRUE(player_->GetTickerById(ticker_id_));

  timeline_->AttachPlayer(player_);
  player_->AttachElementForTicker(element_id_, ticker_id_);

  EXPECT_FALSE(client_.mutators_need_commit());

  const int animation_id =
      AddOpacityTransition(player_.get(), 1., .7f, .3f, false, ticker_id_);

  EXPECT_TRUE(client_.mutators_need_commit());
  client_.set_mutators_need_commit(false);

  player_->PauseAnimationForTicker(animation_id, 1., ticker_id_);
  EXPECT_TRUE(client_.mutators_need_commit());
  client_.set_mutators_need_commit(false);

  player_->RemoveAnimationForTicker(animation_id, ticker_id_);
  EXPECT_TRUE(client_.mutators_need_commit());
  client_.set_mutators_need_commit(false);
}

// If main-thread player switches to another layer within one frame then
// impl-thread player must be switched as well.
TEST_F(AnimationPlayerTest, SwitchToLayer) {
  host_->AddAnimationTimeline(timeline_);
  player_->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id_));
  timeline_->AttachPlayer(player_);
  player_->AttachElementForTicker(element_id_, ticker_id_);

  host_->PushPropertiesTo(host_impl_);

  GetImplTimelineAndPlayerByID();

  EXPECT_EQ(player_->GetTickerById(ticker_id_),
            GetTickerForElementId(element_id_));
  EXPECT_TRUE(player_->GetTickerById(ticker_id_)->element_animations());
  EXPECT_EQ(player_->GetTickerById(ticker_id_)->element_id(), element_id_);

  timeline_impl_ = host_impl_->GetTimelineById(timeline_id_);
  EXPECT_TRUE(timeline_impl_);
  player_impl_ = timeline_impl_->GetPlayerById(player_id_);
  EXPECT_TRUE(player_impl_);
  EXPECT_EQ(player_impl_->GetTickerById(ticker_id_),
            GetImplTickerForLayerId(element_id_));
  EXPECT_TRUE(player_impl_->GetTickerById(ticker_id_)->element_animations());
  EXPECT_EQ(player_impl_->GetTickerById(ticker_id_)->element_id(), element_id_);
  CheckTickerAndTimelineNeedsPushProperties(false, ticker_id_);

  const ElementId new_element_id(NextTestLayerId());
  player_->DetachElement();
  player_->AttachElementForTicker(new_element_id, ticker_id_);

  EXPECT_EQ(player_->GetTickerById(ticker_id_),
            GetTickerForElementId(new_element_id));
  EXPECT_TRUE(player_->GetTickerById(ticker_id_)->element_animations());
  EXPECT_EQ(player_->GetTickerById(ticker_id_)->element_id(), new_element_id);
  CheckTickerAndTimelineNeedsPushProperties(true, ticker_id_);

  host_->PushPropertiesTo(host_impl_);

  EXPECT_EQ(player_impl_->GetTickerById(ticker_id_),
            GetImplTickerForLayerId(new_element_id));
  EXPECT_TRUE(player_impl_->GetTickerById(ticker_id_)->element_animations());
  EXPECT_EQ(player_impl_->GetTickerById(ticker_id_)->element_id(),
            new_element_id);
}

TEST_F(AnimationPlayerTest, ToString) {
  player_->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id_));
  player_->AttachElementForTicker(element_id_, ticker_id_);
  EXPECT_EQ(
      base::StringPrintf("AnimationPlayer{id=%d, element_id=%s, animations=[]}",
                         player_->id(), element_id_.ToString().c_str()),
      player_->ToString());

  player_->AddAnimationForTicker(
      Animation::Create(std::make_unique<FakeFloatAnimationCurve>(15), 42, 73,
                        TargetProperty::OPACITY),
      ticker_id_);
  EXPECT_EQ(base::StringPrintf("AnimationPlayer{id=%d, element_id=%s, "
                               "animations=[Animation{id=42, "
                               "group=73, target_property_id=1, "
                               "run_state=WAITING_FOR_TARGET_AVAILABILITY}]}",
                               player_->id(), element_id_.ToString().c_str()),
            player_->ToString());

  player_->AddAnimationForTicker(
      Animation::Create(std::make_unique<FakeFloatAnimationCurve>(18), 45, 76,
                        TargetProperty::BOUNDS),
      ticker_id_);
  EXPECT_EQ(
      base::StringPrintf(
          "AnimationPlayer{id=%d, element_id=%s, "
          "animations=[Animation{id=42, "
          "group=73, target_property_id=1, "
          "run_state=WAITING_FOR_TARGET_AVAILABILITY}, Animation{id=45, "
          "group=76, "
          "target_property_id=5, run_state=WAITING_FOR_TARGET_AVAILABILITY}]}",
          player_->id(), element_id_.ToString().c_str()),
      player_->ToString());

  TickerId second_ticker_id = player_->NextTickerId();
  ElementId second_element_id(NextTestLayerId());
  player_->AddTicker(base::MakeUnique<AnimationTicker>(second_ticker_id));
  player_->AttachElementForTicker(second_element_id, second_ticker_id);
  player_->AddAnimationForTicker(
      Animation::Create(std::make_unique<FakeFloatAnimationCurve>(20), 48, 78,
                        TargetProperty::OPACITY),
      second_ticker_id);
  EXPECT_EQ(
      base::StringPrintf(
          "AnimationPlayer{id=%d, element_id=%s, "
          "animations=[Animation{id=42, "
          "group=73, target_property_id=1, "
          "run_state=WAITING_FOR_TARGET_AVAILABILITY}, Animation{id=45, "
          "group=76, "
          "target_property_id=5, run_state=WAITING_FOR_TARGET_AVAILABILITY}]"
          ", element_id=%s, "
          "animations=[Animation{id=48, "
          "group=78, target_property_id=1, "
          "run_state=WAITING_FOR_TARGET_AVAILABILITY}]}",
          player_->id(), element_id_.ToString().c_str(),
          second_element_id.ToString().c_str()),
      player_->ToString());
}

TEST_F(AnimationPlayerTest, AddTwoTickersFromTheSameElementToOnePlayerTest) {
  host_->AddAnimationTimeline(timeline_);
  EXPECT_TRUE(timeline_->needs_push_properties());

  TickerId ticker_id1 = player_->NextTickerId();

  player_->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id1));
  ASSERT_TRUE(player_->GetTickerById(ticker_id1));
  EXPECT_FALSE(player_->GetTickerById(ticker_id1)->needs_push_properties());

  TickerId ticker_id2 = player_->NextTickerId();
  player_->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id2));
  ASSERT_TRUE(player_->GetTickerById(ticker_id2));
  EXPECT_FALSE(player_->GetTickerById(ticker_id2)->needs_push_properties());

  timeline_->AttachPlayer(player_);
  EXPECT_FALSE(player_->element_animations(ticker_id1));
  EXPECT_FALSE(player_->element_animations(ticker_id2));
  EXPECT_FALSE(player_->element_id_of_ticker(ticker_id1));
  EXPECT_FALSE(player_->element_id_of_ticker(ticker_id2));
  EXPECT_TRUE(timeline_->needs_push_properties());
  EXPECT_FALSE(player_->GetTickerById(ticker_id1)->needs_push_properties());
  EXPECT_FALSE(player_->GetTickerById(ticker_id2)->needs_push_properties());

  host_->PushPropertiesTo(host_impl_);

  timeline_impl_ = host_impl_->GetTimelineById(timeline_id_);
  EXPECT_TRUE(timeline_impl_);
  player_impl_ = timeline_impl_->GetPlayerById(player_id_);
  EXPECT_TRUE(player_impl_);

  player_->AttachElementForTicker(element_id_, ticker_id1);
  EXPECT_TRUE(player_->element_animations(ticker_id1));
  EXPECT_EQ(player_->element_id_of_ticker(ticker_id1), element_id_);
  EXPECT_TRUE(player_->GetTickerById(ticker_id1)->needs_push_properties());

  EXPECT_FALSE(player_->element_animations(ticker_id2));
  EXPECT_NE(player_->element_id_of_ticker(ticker_id2), element_id_);
  EXPECT_FALSE(player_->GetTickerById(ticker_id2)->needs_push_properties());

  const scoped_refptr<ElementAnimations> element_animations =
      host_->GetElementAnimationsForElementId(element_id_);
  EXPECT_TRUE(element_animations->tickers_list().HasObserver(
      player_->GetTickerById(ticker_id1)));
  EXPECT_FALSE(element_animations->tickers_list().HasObserver(
      player_->GetTickerById(ticker_id2)));

  player_->AttachElementForTicker(element_id_, ticker_id2);
  EXPECT_TRUE(player_->element_animations(ticker_id2));
  EXPECT_EQ(player_->element_id_of_ticker(ticker_id2), element_id_);
  EXPECT_TRUE(player_->GetTickerById(ticker_id2)->needs_push_properties());

  EXPECT_TRUE(element_animations->tickers_list().HasObserver(
      player_->GetTickerById(ticker_id2)));

  host_->PushPropertiesTo(host_impl_);

  const scoped_refptr<ElementAnimations> element_animations_impl =
      host_impl_->GetElementAnimationsForElementId(element_id_);
  EXPECT_TRUE(element_animations_impl->tickers_list().HasObserver(
      player_impl_->GetTickerById(ticker_id1)));
  EXPECT_TRUE(element_animations_impl->tickers_list().HasObserver(
      player_impl_->GetTickerById(ticker_id2)));

  EXPECT_TRUE(player_impl_->element_animations(ticker_id1));
  EXPECT_EQ(player_impl_->element_id_of_ticker(ticker_id1), element_id_);
  EXPECT_TRUE(player_impl_->element_animations(ticker_id2));
  EXPECT_EQ(player_impl_->element_id_of_ticker(ticker_id2), element_id_);

  player_->DetachElement();
  EXPECT_FALSE(player_->element_animations(ticker_id1));
  EXPECT_FALSE(player_->element_id_of_ticker(ticker_id1));
  EXPECT_FALSE(element_animations->tickers_list().HasObserver(
      player_->GetTickerById(ticker_id1)));

  EXPECT_FALSE(player_->element_animations(ticker_id2));
  EXPECT_FALSE(player_->element_id_of_ticker(ticker_id2));
  EXPECT_FALSE(element_animations->tickers_list().HasObserver(
      player_->GetTickerById(ticker_id2)));

  EXPECT_TRUE(element_animations_impl->tickers_list().HasObserver(
      player_impl_->GetTickerById(ticker_id1)));
  EXPECT_TRUE(element_animations_impl->tickers_list().HasObserver(
      player_impl_->GetTickerById(ticker_id2)));

  host_->PushPropertiesTo(host_impl_);

  EXPECT_FALSE(player_impl_->element_animations(ticker_id1));
  EXPECT_FALSE(player_impl_->element_id_of_ticker(ticker_id1));
  EXPECT_FALSE(element_animations_impl->tickers_list().HasObserver(
      player_impl_->GetTickerById(ticker_id1)));
  EXPECT_FALSE(player_impl_->element_animations(ticker_id2));
  EXPECT_FALSE(player_impl_->element_id_of_ticker(ticker_id2));
  EXPECT_FALSE(element_animations_impl->tickers_list().HasObserver(
      player_impl_->GetTickerById(ticker_id2)));

  timeline_->DetachPlayer(player_);
  EXPECT_FALSE(player_->animation_timeline());

  EXPECT_FALSE(player_->element_animations(ticker_id1));
  EXPECT_FALSE(player_->element_id_of_ticker(ticker_id1));
  EXPECT_FALSE(player_->element_animations(ticker_id2));
  EXPECT_FALSE(player_->element_id_of_ticker(ticker_id2));

  EXPECT_TRUE(timeline_->needs_push_properties());
  EXPECT_FALSE(player_->GetTickerById(ticker_id1)->needs_push_properties());
  EXPECT_FALSE(player_->GetTickerById(ticker_id2)->needs_push_properties());
}

TEST_F(AnimationPlayerTest, AddTwoTickersFromDifferentElementsToOnePlayerTest) {
  host_->AddAnimationTimeline(timeline_);

  TickerId ticker_id1 = player_->NextTickerId();

  player_->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id1));
  ASSERT_TRUE(player_->GetTickerById(ticker_id1));
  EXPECT_FALSE(player_->GetTickerById(ticker_id1)->needs_push_properties());

  TickerId ticker_id2 = player_->NextTickerId();
  player_->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id2));
  ASSERT_TRUE(player_->GetTickerById(ticker_id2));
  EXPECT_FALSE(player_->GetTickerById(ticker_id2)->needs_push_properties());

  EXPECT_FALSE(player_->element_animations(ticker_id1));
  EXPECT_FALSE(player_->element_animations(ticker_id2));
  EXPECT_FALSE(player_->element_id_of_ticker(ticker_id1));
  EXPECT_FALSE(player_->element_id_of_ticker(ticker_id2));
  EXPECT_TRUE(timeline_->needs_push_properties());
  EXPECT_FALSE(player_->GetTickerById(ticker_id1)->needs_push_properties());
  EXPECT_FALSE(player_->GetTickerById(ticker_id2)->needs_push_properties());

  ElementId element1(NextTestLayerId());
  ElementId element2(NextTestLayerId());

  player_->AttachElementForTicker(element1, ticker_id1);
  player_->AttachElementForTicker(element2, ticker_id2);

  EXPECT_FALSE(player_->animation_timeline());

  scoped_refptr<ElementAnimations> element_animations =
      host_->GetElementAnimationsForElementId(element1);
  EXPECT_FALSE(element_animations);
  EXPECT_FALSE(player_->GetTickerById(ticker_id1)->element_animations());
  EXPECT_EQ(player_->GetTickerById(ticker_id1)->element_id(), element1);
  EXPECT_FALSE(player_->GetTickerById(ticker_id1)->needs_push_properties());

  timeline_->AttachPlayer(player_);
  EXPECT_EQ(timeline_, player_->animation_timeline());
  EXPECT_TRUE(player_->GetTickerById(ticker_id1)->element_animations());
  EXPECT_EQ(player_->GetTickerById(ticker_id1)->element_id(), element1);
  EXPECT_TRUE(player_->GetTickerById(ticker_id1)->needs_push_properties());

  element_animations = host_->GetElementAnimationsForElementId(element1);
  EXPECT_TRUE(element_animations->tickers_list().HasObserver(
      player_->GetTickerById(ticker_id1)));
  EXPECT_FALSE(element_animations->tickers_list().HasObserver(
      player_->GetTickerById(ticker_id2)));

  element_animations = host_->GetElementAnimationsForElementId(element2);
  EXPECT_TRUE(element_animations->tickers_list().HasObserver(
      player_->GetTickerById(ticker_id2)));

  player_->DetachElement();
  EXPECT_TRUE(player_->animation_timeline());
  EXPECT_FALSE(element_animations->tickers_list().HasObserver(
      player_->GetTickerById(ticker_id2)));
  EXPECT_FALSE(player_->GetTickerById(ticker_id1)->element_animations());
  EXPECT_FALSE(player_->GetTickerById(ticker_id1)->element_id());
  EXPECT_TRUE(player_->GetTickerById(ticker_id1)->needs_push_properties());
  EXPECT_FALSE(player_->GetTickerById(ticker_id2)->element_animations());
  EXPECT_FALSE(player_->GetTickerById(ticker_id2)->element_id());
  EXPECT_TRUE(player_->GetTickerById(ticker_id2)->needs_push_properties());

  // Removing player from timeline detaches layer.
  timeline_->DetachPlayer(player_);
  EXPECT_FALSE(player_->animation_timeline());
  EXPECT_FALSE(player_->GetTickerById(ticker_id1)->element_animations());
  EXPECT_FALSE(player_->GetTickerById(ticker_id1)->element_id());
  EXPECT_TRUE(player_->GetTickerById(ticker_id1)->needs_push_properties());
  EXPECT_FALSE(player_->GetTickerById(ticker_id2)->element_animations());
  EXPECT_FALSE(player_->GetTickerById(ticker_id2)->element_id());
  EXPECT_TRUE(player_->GetTickerById(ticker_id2)->needs_push_properties());
}

TEST_F(AnimationPlayerTest, TickingAnimationsFromTwoTickers) {
  TestAnimationDelegate delegate1;

  client_.RegisterElement(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElement(element_id_, ElementListType::PENDING);
  client_impl_.RegisterElement(element_id_, ElementListType::ACTIVE);

  TickerId ticker_id1 = player_->NextTickerId();

  player_->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id1));
  ASSERT_TRUE(player_->GetTickerById(ticker_id1));
  EXPECT_FALSE(player_->GetTickerById(ticker_id1)->needs_push_properties());

  TickerId ticker_id2 = player_->NextTickerId();
  player_->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id2));
  ASSERT_TRUE(player_->GetTickerById(ticker_id2));
  EXPECT_FALSE(player_->GetTickerById(ticker_id2)->needs_push_properties());

  host_->AddAnimationTimeline(timeline_);

  timeline_->AttachPlayer(player_);
  EXPECT_TRUE(timeline_->needs_push_properties());

  player_->set_animation_delegate(&delegate1);

  player_->AttachElementForTicker(element_id_, ticker_id1);
  player_->AttachElementForTicker(element_id_, ticker_id2);

  const float start_opacity = .7f;
  const float end_opacity = .3f;

  const int transform_x = 10;
  const int transform_y = 20;

  const double duration = 1.;

  AddOpacityTransition(player_.get(), duration, start_opacity, end_opacity,
                       false, ticker_id1);
  AddAnimatedTransform(player_.get(), duration, transform_x, transform_y,
                       ticker_id2);
  host_->PushPropertiesTo(host_impl_);
  host_impl_->ActivateAnimations();

  EXPECT_FALSE(delegate1.started());
  EXPECT_FALSE(delegate1.finished());

  base::TimeTicks time;
  time += base::TimeDelta::FromSecondsD(0.1);
  TickAnimationsTransferEvents(time, 2u);

  EXPECT_TRUE(delegate1.started());
  EXPECT_FALSE(delegate1.finished());

  EXPECT_FALSE(player_->GetTickerById(ticker_id1)->needs_push_properties());
  EXPECT_FALSE(player_->GetTickerById(ticker_id2)->needs_push_properties());

  time += base::TimeDelta::FromSecondsD(duration);
  TickAnimationsTransferEvents(time, 2u);

  EXPECT_TRUE(delegate1.finished());

  EXPECT_TRUE(player_->GetTickerById(ticker_id1)->needs_push_properties());
  EXPECT_TRUE(player_->GetTickerById(ticker_id2)->needs_push_properties());

  client_.ExpectOpacityPropertyMutated(element_id_, ElementListType::ACTIVE,
                                       end_opacity);
  client_.ExpectTransformPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         transform_x, transform_y);

  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::ACTIVE, end_opacity);
  client_impl_.ExpectTransformPropertyMutated(
      element_id_, ElementListType::ACTIVE, transform_x, transform_y);

  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::PENDING, end_opacity);
  client_impl_.ExpectTransformPropertyMutated(
      element_id_, ElementListType::PENDING, transform_x, transform_y);
}

TEST_F(AnimationPlayerTest, TickerSyncToImplTest) {
  host_->AddAnimationTimeline(timeline_);
  EXPECT_TRUE(timeline_->needs_push_properties());
  timeline_->AttachPlayer(player_);

  TickerId ticker_id1 = player_->NextTickerId();
  player_->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id1));
  EXPECT_TRUE(player_->GetTickerById(ticker_id1));
  EXPECT_FALSE(player_->GetTickerById(ticker_id1)->needs_push_properties());

  host_->PushPropertiesTo(host_impl_);

  timeline_impl_ = host_impl_->GetTimelineById(timeline_id_);
  EXPECT_TRUE(timeline_impl_);
  player_impl_ = timeline_impl_->GetPlayerById(player_id_);
  EXPECT_TRUE(player_impl_);
  EXPECT_TRUE(player_impl_->GetTickerById(ticker_id1));

  EXPECT_FALSE(timeline_->needs_push_properties());

  TickerId ticker_id2 = player_->NextTickerId();
  player_->AddTicker(base::MakeUnique<AnimationTicker>(ticker_id2));
  EXPECT_TRUE(timeline_->needs_push_properties());

  host_->PushPropertiesTo(host_impl_);

  EXPECT_TRUE(player_impl_->GetTickerById(ticker_id2));
}

}  // namespace
}  // namespace cc
