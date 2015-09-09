// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/user_model.h"

#include "base/test/simple_test_tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace scheduler {

class UserModelTest : public testing::Test {
 public:
  UserModelTest() {}
  ~UserModelTest() override {}

  void SetUp() override {
    clock_.reset(new base::SimpleTestTickClock());
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));

    user_model_.reset(new UserModel());
  }

 protected:
  static base::TimeDelta priority_escalation_after_input_duration() {
    return base::TimeDelta::FromMilliseconds(
        UserModel::kGestureEstimationLimitMillis);
  }

  static base::TimeDelta subsequent_input_expected_after_input_duration() {
    return base::TimeDelta::FromMilliseconds(
        UserModel::kExpectSubsequentGestureMillis);
  }

  static base::TimeDelta minimum_typical_scroll_duration_millis() {
    return base::TimeDelta::FromMilliseconds(
        UserModel::kMinimumTypicalScrollDurationMillis);
  }

  scoped_ptr<base::SimpleTestTickClock> clock_;
  scoped_ptr<UserModel> user_model_;
};

TEST_F(UserModelTest, TimeLeftInUserGesture_NoInput) {
  EXPECT_EQ(base::TimeDelta(),
            user_model_->TimeLeftInUserGesture(clock_->NowTicks()));
}

TEST_F(UserModelTest, TimeLeftInUserGesture_ImmediatelyAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::TouchStart, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());
  EXPECT_EQ(priority_escalation_after_input_duration(),
            user_model_->TimeLeftInUserGesture(clock_->NowTicks()));
}

TEST_F(UserModelTest, TimeLeftInUserGesture_ShortlyAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::TouchStart, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());
  base::TimeDelta delta(base::TimeDelta::FromMilliseconds(10));
  clock_->Advance(delta);
  EXPECT_EQ(priority_escalation_after_input_duration() - delta,
            user_model_->TimeLeftInUserGesture(clock_->NowTicks()));
}

TEST_F(UserModelTest, TimeLeftInUserGesture_LongAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::TouchStart, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());
  clock_->Advance(priority_escalation_after_input_duration() * 2);
  EXPECT_EQ(base::TimeDelta(),
            user_model_->TimeLeftInUserGesture(clock_->NowTicks()));
}

TEST_F(UserModelTest, DidFinishProcessingInputEvent_Delayed) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::TouchStart, clock_->NowTicks());
  clock_->Advance(priority_escalation_after_input_duration() * 10);

  EXPECT_EQ(priority_escalation_after_input_duration(),
            user_model_->TimeLeftInUserGesture(clock_->NowTicks()));

  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());
  base::TimeDelta delta(base::TimeDelta::FromMilliseconds(10));
  clock_->Advance(delta);

  EXPECT_EQ(priority_escalation_after_input_duration() - delta,
            user_model_->TimeLeftInUserGesture(clock_->NowTicks()));
}

TEST_F(UserModelTest, GestureExpectedSoon_UseCase_NONE_NoRecentInput) {
  base::TimeDelta prediction_valid_duration;
  EXPECT_FALSE(user_model_->IsGestureExpectedSoon(
      RendererScheduler::UseCase::NONE, clock_->NowTicks(),
      &prediction_valid_duration));
  EXPECT_EQ(base::TimeDelta(), prediction_valid_duration);
}

TEST_F(UserModelTest, GestureExpectedSoon_UseCase_NONE_ImmediatelyAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::GestureScrollEnd, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta prediction_valid_duration;
  EXPECT_TRUE(user_model_->IsGestureExpectedSoon(
      RendererScheduler::UseCase::NONE, clock_->NowTicks(),
      &prediction_valid_duration));
  EXPECT_EQ(subsequent_input_expected_after_input_duration(),
            prediction_valid_duration);
}

TEST_F(UserModelTest, GestureExpectedSoon_UseCase_NONE_ShortlyAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::GestureScrollEnd, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta delta(base::TimeDelta::FromMilliseconds(10));
  clock_->Advance(delta);

  base::TimeDelta prediction_valid_duration;
  EXPECT_TRUE(user_model_->IsGestureExpectedSoon(
      RendererScheduler::UseCase::NONE, clock_->NowTicks(),
      &prediction_valid_duration));
  EXPECT_EQ(subsequent_input_expected_after_input_duration() - delta,
            prediction_valid_duration);
}

TEST_F(UserModelTest, GestureExpectedSoon_UseCase_NONE_LongAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::GestureScrollEnd, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());
  clock_->Advance(subsequent_input_expected_after_input_duration() * 2);

  base::TimeDelta prediction_valid_duration;
  EXPECT_FALSE(user_model_->IsGestureExpectedSoon(
      RendererScheduler::UseCase::NONE, clock_->NowTicks(),
      &prediction_valid_duration));
  EXPECT_EQ(base::TimeDelta(), prediction_valid_duration);
}

TEST_F(UserModelTest,
       GestureExpectedSoon_COMPOSITOR_GESTURE_ImmediatelyAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::TouchStart, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta prediction_valid_duration;
  EXPECT_FALSE(user_model_->IsGestureExpectedSoon(
      RendererScheduler::UseCase::COMPOSITOR_GESTURE, clock_->NowTicks(),
      &prediction_valid_duration));
  EXPECT_EQ(minimum_typical_scroll_duration_millis(),
            prediction_valid_duration);
}

TEST_F(UserModelTest,
       GestureExpectedSoon_COMPOSITOR_GESTURE_ShortlyAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::TouchStart, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta delta(base::TimeDelta::FromMilliseconds(10));
  clock_->Advance(delta);

  base::TimeDelta prediction_valid_duration;
  EXPECT_FALSE(user_model_->IsGestureExpectedSoon(
      RendererScheduler::UseCase::COMPOSITOR_GESTURE, clock_->NowTicks(),
      &prediction_valid_duration));
  EXPECT_EQ(minimum_typical_scroll_duration_millis() - delta,
            prediction_valid_duration);
}

TEST_F(UserModelTest, GestureExpectedSoon_COMPOSITOR_GESTURE_LongAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::TouchStart, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  clock_->Advance(minimum_typical_scroll_duration_millis() * 2);

  base::TimeDelta prediction_valid_duration;
  // Note this isn't a bug, the UseCase will change to NONE eventually so it's
  // OK for this to always be true after minimum_typical_scroll_duration_millis
  EXPECT_TRUE(user_model_->IsGestureExpectedSoon(
      RendererScheduler::UseCase::COMPOSITOR_GESTURE, clock_->NowTicks(),
      &prediction_valid_duration));
  EXPECT_EQ(base::TimeDelta(), prediction_valid_duration);
}

TEST_F(UserModelTest,
       GestureExpectedSoon_MAIN_THREAD_GESTURE_ImmediatelyAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::TouchStart, clock_->NowTicks());

  // DidFinishProcessingInputEvent is always a little bit delayed.
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(5));
  clock_->Advance(delay);
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta prediction_valid_duration;
  EXPECT_FALSE(user_model_->IsGestureExpectedSoon(
      RendererScheduler::UseCase::MAIN_THREAD_GESTURE, clock_->NowTicks(),
      &prediction_valid_duration));
  EXPECT_EQ(minimum_typical_scroll_duration_millis() - delay,
            prediction_valid_duration);
}

TEST_F(UserModelTest,
       GestureExpectedSoon_MAIN_THREAD_GESTURE_ShortlyAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::TouchStart, clock_->NowTicks());
  // DidFinishProcessingInputEvent is always a little bit delayed.
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(5));
  clock_->Advance(delay);
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta delta(base::TimeDelta::FromMilliseconds(10));
  clock_->Advance(delta);

  base::TimeDelta prediction_valid_duration;
  EXPECT_FALSE(user_model_->IsGestureExpectedSoon(
      RendererScheduler::UseCase::MAIN_THREAD_GESTURE, clock_->NowTicks(),
      &prediction_valid_duration));
  EXPECT_EQ(minimum_typical_scroll_duration_millis() - delta - delay,
            prediction_valid_duration);
}

TEST_F(UserModelTest, GestureExpectedSoon_MAIN_THREAD_GESTURE_LongAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::TouchStart, clock_->NowTicks());

  // DidFinishProcessingInputEvent is always a little bit delayed.
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(5));
  clock_->Advance(delay);
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  clock_->Advance(minimum_typical_scroll_duration_millis() * 2);

  base::TimeDelta prediction_valid_duration;
  // Note this isn't a bug, the UseCase will change to NONE eventually so it's
  // OK for this to always be true after minimum_typical_scroll_duration_millis
  EXPECT_TRUE(user_model_->IsGestureExpectedSoon(
      RendererScheduler::UseCase::MAIN_THREAD_GESTURE, clock_->NowTicks(),
      &prediction_valid_duration));
  EXPECT_EQ(base::TimeDelta(), prediction_valid_duration);
}

TEST_F(UserModelTest,
       GestureExpectedSoon_UseCase_NONE_ShortlyAfterInput_GestureScrollBegin) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::GestureScrollBegin, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta delta(base::TimeDelta::FromMilliseconds(10));
  clock_->Advance(delta);

  base::TimeDelta prediction_valid_duration;
  EXPECT_TRUE(user_model_->IsGestureExpectedSoon(
      RendererScheduler::UseCase::NONE, clock_->NowTicks(),
      &prediction_valid_duration));
  EXPECT_EQ(subsequent_input_expected_after_input_duration() - delta,
            prediction_valid_duration);
}

TEST_F(UserModelTest,
       GestureExpectedSoon_UseCase_NONE_ShortlyAfterInput_GesturePinchBegin) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::GestureScrollBegin, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta delta(base::TimeDelta::FromMilliseconds(10));
  clock_->Advance(delta);

  base::TimeDelta prediction_valid_duration;
  EXPECT_TRUE(user_model_->IsGestureExpectedSoon(
      RendererScheduler::UseCase::NONE, clock_->NowTicks(),
      &prediction_valid_duration));
  EXPECT_EQ(subsequent_input_expected_after_input_duration() - delta,
            prediction_valid_duration);
}

TEST_F(UserModelTest,
       GestureExpectedSoon_UseCase_NONE_ShortlyAfterInput_GestureTap) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::GestureTap, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta delta(base::TimeDelta::FromMilliseconds(10));
  clock_->Advance(delta);

  base::TimeDelta prediction_valid_duration;
  EXPECT_FALSE(user_model_->IsGestureExpectedSoon(
      RendererScheduler::UseCase::NONE, clock_->NowTicks(),
      &prediction_valid_duration));
  EXPECT_EQ(base::TimeDelta(), prediction_valid_duration);
}

}  // namespace scheduler
