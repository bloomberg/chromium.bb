// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_pointer_action.h"
#include "content/browser/renderer_host/input/synthetic_touch_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebMouseEvent;
using blink::WebTouchPoint;

namespace content {

namespace {

WebTouchPoint::State ToWebTouchPointState(
    SyntheticPointerActionParams::PointerActionType action_type) {
  switch (action_type) {
    case SyntheticPointerActionParams::PointerActionType::PRESS:
      return WebTouchPoint::StatePressed;
    case SyntheticPointerActionParams::PointerActionType::MOVE:
      return WebTouchPoint::StateMoved;
    case SyntheticPointerActionParams::PointerActionType::RELEASE:
      return WebTouchPoint::StateReleased;
    case SyntheticPointerActionParams::PointerActionType::IDLE:
      return WebTouchPoint::StateStationary;
    case SyntheticPointerActionParams::PointerActionType::FINISH:
    case SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED:
      NOTREACHED()
          << "Invalid SyntheticPointerActionParams::PointerActionType.";
      return WebTouchPoint::StateUndefined;
  }
  NOTREACHED() << "Invalid SyntheticPointerActionParams::PointerActionType.";
  return WebTouchPoint::StateUndefined;
}

WebInputEvent::Type ToWebMouseEventType(
    SyntheticPointerActionParams::PointerActionType action_type) {
  switch (action_type) {
    case SyntheticPointerActionParams::PointerActionType::PRESS:
      return WebInputEvent::MouseDown;
    case SyntheticPointerActionParams::PointerActionType::MOVE:
      return WebInputEvent::MouseMove;
    case SyntheticPointerActionParams::PointerActionType::RELEASE:
      return WebInputEvent::MouseUp;
    case SyntheticPointerActionParams::PointerActionType::IDLE:
    case SyntheticPointerActionParams::PointerActionType::FINISH:
    case SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED:
      NOTREACHED()
          << "Invalid SyntheticPointerActionParams::PointerActionType.";
      return WebInputEvent::Undefined;
  }
  NOTREACHED() << "Invalid SyntheticPointerActionParams::PointerActionType.";
  return WebInputEvent::Undefined;
}

class MockSyntheticPointerActionTarget : public SyntheticGestureTarget {
 public:
  MockSyntheticPointerActionTarget() {}
  ~MockSyntheticPointerActionTarget() override {}

  // SyntheticGestureTarget:
  void SetNeedsFlush() override { NOTIMPLEMENTED(); }

  base::TimeDelta PointerAssumedStoppedTime() const override {
    NOTIMPLEMENTED();
    return base::TimeDelta();
  }

  float GetTouchSlopInDips() const override {
    NOTIMPLEMENTED();
    return 0.0f;
  }

  float GetMinScalingSpanInDips() const override {
    NOTIMPLEMENTED();
    return 0.0f;
  }

  WebInputEvent::Type type() const { return type_; }

 protected:
  WebInputEvent::Type type_;
};

class MockSyntheticPointerTouchActionTarget
    : public MockSyntheticPointerActionTarget {
 public:
  MockSyntheticPointerTouchActionTarget() {}
  ~MockSyntheticPointerTouchActionTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    DCHECK(WebInputEvent::isTouchEventType(event.type));
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    type_ = touch_event.type;
    for (size_t i = 0; i < touch_event.touchesLength; ++i) {
      indexes_[i] = touch_event.touches[i].id;
      positions_[i] = gfx::PointF(touch_event.touches[i].position);
      states_[i] = touch_event.touches[i].state;
    }
    touch_length_ = touch_event.touchesLength;
  }

  testing::AssertionResult SyntheticTouchActionDispatchedCorrectly(
      const SyntheticPointerActionParams& param,
      int index) {
    if (param.pointer_action_type() ==
            SyntheticPointerActionParams::PointerActionType::PRESS ||
        param.pointer_action_type() ==
            SyntheticPointerActionParams::PointerActionType::MOVE) {
      if (indexes_[index] != param.index()) {
        return testing::AssertionFailure()
               << "Pointer index at index " << index << " was "
               << indexes_[index] << ", expected " << param.index() << ".";
      }

      if (positions_[index] != param.position()) {
        return testing::AssertionFailure()
               << "Pointer position at index " << index << " was "
               << positions_[index].ToString() << ", expected "
               << param.position().ToString() << ".";
      }
    }

    if (states_[index] != ToWebTouchPointState(param.pointer_action_type())) {
      return testing::AssertionFailure()
             << "Pointer states at index " << index << " was " << states_[index]
             << ", expected "
             << ToWebTouchPointState(param.pointer_action_type()) << ".";
    }
    return testing::AssertionSuccess();
  }

  testing::AssertionResult SyntheticTouchActionListDispatchedCorrectly(
      const std::vector<SyntheticPointerActionParams>& params_list) {
    if (touch_length_ != params_list.size()) {
      return testing::AssertionFailure() << "Touch point length was "
                                         << touch_length_ << ", expected "
                                         << params_list.size() << ".";
    }

    testing::AssertionResult result = testing::AssertionSuccess();
    for (size_t i = 0; i < params_list.size(); ++i) {
      result = SyntheticTouchActionDispatchedCorrectly(params_list[i], i);
      if (result == testing::AssertionFailure())
        return result;
    }
    return testing::AssertionSuccess();
  }

  SyntheticGestureParams::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const override {
    return SyntheticGestureParams::TOUCH_INPUT;
  }

 private:
  gfx::PointF positions_[WebTouchEvent::kTouchesLengthCap];
  unsigned touch_length_;
  int indexes_[WebTouchEvent::kTouchesLengthCap];
  WebTouchPoint::State states_[WebTouchEvent::kTouchesLengthCap];
};

class MockSyntheticPointerMouseActionTarget
    : public MockSyntheticPointerActionTarget {
 public:
  MockSyntheticPointerMouseActionTarget() {}
  ~MockSyntheticPointerMouseActionTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    DCHECK(WebInputEvent::isMouseEventType(event.type));
    const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);
    type_ = mouse_event.type;
    position_ = gfx::PointF(mouse_event.x, mouse_event.y);
    clickCount_ = mouse_event.clickCount;
    button_ = mouse_event.button;
  }

  testing::AssertionResult SyntheticMouseActionDispatchedCorrectly(
      const SyntheticPointerActionParams& param,
      int click_count) {
    if (type() != ToWebMouseEventType(param.pointer_action_type())) {
      return testing::AssertionFailure()
             << "Pointer type was " << WebInputEvent::GetName(type())
             << ", expected " << WebInputEvent::GetName(ToWebMouseEventType(
                                     param.pointer_action_type()))
             << ".";
    }

    if (clickCount() != click_count) {
      return testing::AssertionFailure() << "Pointer click count was "
                                         << clickCount() << ", expected "
                                         << click_count << ".";
    }

    if (clickCount() == 1 && button() != WebMouseEvent::Button::Left) {
      return testing::AssertionFailure()
             << "Pointer button was " << (int)button() << ", expected "
             << (int)WebMouseEvent::Button::Left << ".";
    }

    if (clickCount() == 0 && button() != WebMouseEvent::Button::NoButton) {
      return testing::AssertionFailure()
             << "Pointer button was " << (int)button() << ", expected "
             << (int)WebMouseEvent::Button::NoButton << ".";
    }

    if ((param.pointer_action_type() ==
             SyntheticPointerActionParams::PointerActionType::PRESS ||
         param.pointer_action_type() ==
             SyntheticPointerActionParams::PointerActionType::MOVE) &&
        position() != param.position()) {
      return testing::AssertionFailure()
             << "Pointer position was " << position().ToString()
             << ", expected " << param.position().ToString() << ".";
    }
    return testing::AssertionSuccess();
  }

  SyntheticGestureParams::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const override {
    return SyntheticGestureParams::MOUSE_INPUT;
  }

  gfx::PointF position() const { return position_; }
  int clickCount() const { return clickCount_; }
  WebMouseEvent::Button button() const { return button_; }

 private:
  gfx::PointF position_;
  int clickCount_;
  WebMouseEvent::Button button_;
};

class SyntheticPointerActionTest : public testing::Test {
 public:
  SyntheticPointerActionTest() {
    action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
    num_success_ = 0;
    num_failure_ = 0;
  }
  ~SyntheticPointerActionTest() override {}

 protected:
  template <typename MockGestureTarget>
  void CreateSyntheticPointerActionTarget() {
    target_.reset(new MockGestureTarget());
    synthetic_pointer_driver_ = SyntheticPointerDriver::Create(
        target_->GetDefaultSyntheticGestureSourceType());
  }

  void ForwardSyntheticPointerAction() {
    pointer_action_.reset(new SyntheticPointerAction(
        action_param_list_.get(), synthetic_pointer_driver_.get()));

    SyntheticGesture::Result result = pointer_action_->ForwardInputEvents(
        base::TimeTicks::Now(), target_.get());

    if (result == SyntheticGesture::GESTURE_FINISHED)
      num_success_++;
    else
      num_failure_++;
  }

  int num_success_;
  int num_failure_;
  std::unique_ptr<MockSyntheticPointerActionTarget> target_;
  std::unique_ptr<SyntheticGesture> pointer_action_;
  std::unique_ptr<SyntheticPointerDriver> synthetic_pointer_driver_;
  std::unique_ptr<std::vector<SyntheticPointerActionParams>> action_param_list_;
};

TEST_F(SyntheticPointerActionTest, PointerTouchAction) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerTouchActionTarget>();

  // Send a touch press for one finger.
  SyntheticPointerActionParams params0 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS,
      SyntheticGestureParams::TOUCH_INPUT);
  params0.set_position(gfx::PointF(54, 89));
  action_param_list_->push_back(params0);
  ForwardSyntheticPointerAction();

  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      *action_param_list_.get()));

  // Send a touch move for the first finger and a touch press for the second
  // finger.
  action_param_list_->at(0).set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  action_param_list_->at(0).set_position(gfx::PointF(133, 156));
  SyntheticPointerActionParams params1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS,
      SyntheticGestureParams::TOUCH_INPUT);
  params1.set_position(gfx::PointF(79, 132));
  action_param_list_->push_back(params1);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  // The type of the SyntheticWebTouchEvent is the action of the last finger.
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      *action_param_list_.get()));

  // Send a touch move for the second finger.
  action_param_list_->at(1).set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  action_param_list_->at(1).set_position(gfx::PointF(87, 253));
  ForwardSyntheticPointerAction();

  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchMove);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      *action_param_list_.get()));

  // Send touch releases for both fingers.
  action_param_list_->at(0).set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  action_param_list_->at(1).set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(4, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchEnd);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      *action_param_list_.get()));
}

TEST_F(SyntheticPointerActionTest, PointerTouchActionWithIdle) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerTouchActionTarget>();
  int count_success = 1;
  // Send a touch press for one finger.
  SyntheticPointerActionParams params0 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS,
      SyntheticGestureParams::TOUCH_INPUT);
  params0.set_position(gfx::PointF(54, 89));
  action_param_list_->push_back(params0);
  ForwardSyntheticPointerAction();

  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_.get());
  EXPECT_EQ(count_success++, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      *action_param_list_.get()));

  SyntheticPointerActionParams params1;
  params1.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  action_param_list_->push_back(params1);
  for (int i = 0; i < 3; ++i) {
    // Send a touch press for the second finger and not move the first finger.
    action_param_list_->at(0).set_pointer_action_type(
        SyntheticPointerActionParams::PointerActionType::IDLE);
    action_param_list_->at(1).set_pointer_action_type(
        SyntheticPointerActionParams::PointerActionType::PRESS);
    action_param_list_->at(1).set_position(gfx::PointF(123, 69));
    ForwardSyntheticPointerAction();

    EXPECT_EQ(count_success++, num_success_);
    EXPECT_EQ(0, num_failure_);
    // The type of the SyntheticWebTouchEvent is the action of the last finger.
    EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
    EXPECT_TRUE(
        pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
            *action_param_list_.get()));

    // Send a touch release for the second finger and not move the first finger.
    action_param_list_->at(0).set_pointer_action_type(
        SyntheticPointerActionParams::PointerActionType::IDLE);
    action_param_list_->at(1).set_pointer_action_type(
        SyntheticPointerActionParams::PointerActionType::RELEASE);

    ForwardSyntheticPointerAction();

    EXPECT_EQ(count_success++, num_success_);
    EXPECT_EQ(0, num_failure_);
    // The type of the SyntheticWebTouchEvent is the action of the last finger.
    EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchEnd);
    EXPECT_TRUE(
        pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
            *action_param_list_.get()));
  }
}

TEST_F(SyntheticPointerActionTest, PointerTouchActionSourceTypeInvalid) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerTouchActionTarget>();

  // Users' gesture source type does not match with the touch action.
  SyntheticPointerActionParams params = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS,
      SyntheticGestureParams::MOUSE_INPUT);
  params.set_position(gfx::PointF(54, 89));
  action_param_list_->push_back(params);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(0, num_success_);
  EXPECT_EQ(1, num_failure_);

  params = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS,
      SyntheticGestureParams::TOUCH_INPUT);
  params.set_position(gfx::PointF(54, 89));
  action_param_list_->at(0) = params;
  ForwardSyntheticPointerAction();

  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(1, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      *action_param_list_.get()));
}

TEST_F(SyntheticPointerActionTest, PointerTouchActionTypeInvalid) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerTouchActionTarget>();

  // Cannot send a touch move or touch release without sending a touch press
  // first.
  SyntheticPointerActionParams params = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE,
      SyntheticGestureParams::TOUCH_INPUT);
  params.set_position(gfx::PointF(54, 89));
  action_param_list_->push_back(params);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(0, num_success_);
  EXPECT_EQ(1, num_failure_);

  action_param_list_->at(0).set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(0, num_success_);
  EXPECT_EQ(2, num_failure_);

  // Send a touch press for one finger.
  action_param_list_->at(0).set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  ForwardSyntheticPointerAction();

  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(2, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      *action_param_list_.get()));

  // Cannot send a touch press again without releasing the finger.
  action_param_list_->at(0).gesture_source_type =
      SyntheticGestureParams::TOUCH_INPUT;
  action_param_list_->at(0).set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(3, num_failure_);
}

TEST_F(SyntheticPointerActionTest, PointerMouseAction) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerMouseActionTarget>();

  // Send a mouse move.
  SyntheticPointerActionParams params = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE,
      SyntheticGestureParams::MOUSE_INPUT);
  params.set_position(gfx::PointF(189, 62));
  action_param_list_->push_back(params);
  ForwardSyntheticPointerAction();

  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      action_param_list_->at(0), 0));

  // Send a mouse down.
  action_param_list_->at(0).set_position(gfx::PointF(189, 62));
  action_param_list_->at(0).set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      action_param_list_->at(0), 1));

  // Send a mouse drag.
  action_param_list_->at(0).set_position(gfx::PointF(326, 298));
  action_param_list_->at(0).set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      action_param_list_->at(0), 1));

  // Send a mouse up.
  action_param_list_->at(0).set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(4, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      action_param_list_->at(0), 1));
}

TEST_F(SyntheticPointerActionTest, PointerMouseActionSourceTypeInvalid) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerMouseActionTarget>();

  // Users' gesture source type does not match with the mouse action.
  SyntheticPointerActionParams params = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS,
      SyntheticGestureParams::TOUCH_INPUT);
  params.set_position(gfx::PointF(54, 89));
  action_param_list_->push_back(params);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(0, num_success_);
  EXPECT_EQ(1, num_failure_);

  params = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS,
      SyntheticGestureParams::MOUSE_INPUT);
  params.set_position(gfx::PointF(54, 89));
  action_param_list_->at(0) = params;
  ForwardSyntheticPointerAction();

  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(1, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      action_param_list_->at(0), 1));
}

TEST_F(SyntheticPointerActionTest, PointerMouseActionTypeInvalid) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerMouseActionTarget>();

  // Send a mouse move.
  SyntheticPointerActionParams params = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE,
      SyntheticGestureParams::MOUSE_INPUT);
  params.set_position(gfx::PointF(189, 62));
  action_param_list_->push_back(params);
  ForwardSyntheticPointerAction();

  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      action_param_list_->at(0), 0));

  // Cannot send a mouse up without sending a mouse down first.
  action_param_list_->at(0).set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(1, num_failure_);

  // Send a mouse down for one finger.
  action_param_list_->at(0).set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(1, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      action_param_list_->at(0), 1));

  // Cannot send a mouse down again without releasing the mouse button.
  action_param_list_->at(0).set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(2, num_failure_);
}

}  // namespace

}  // namespace content