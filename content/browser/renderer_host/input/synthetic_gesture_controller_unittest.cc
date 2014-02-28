// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_controller.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_pinch_gesture.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"
#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/point.h"
#include "ui/gfx/point_f.h"
#include "ui/gfx/vector2d.h"
#include "ui/gfx/vector2d_f.h"

using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;

namespace content {

namespace {

const int kFlushInputRateInMs = 16;
const int kPointerAssumedStoppedTimeMs = 43;
const int kTouchSlopInDips = 7;

class MockSyntheticGesture : public SyntheticGesture {
 public:
  MockSyntheticGesture(bool* finished, int num_steps)
      : finished_(finished),
        num_steps_(num_steps),
        step_count_(0) {
    *finished_ = false;
  }
  virtual ~MockSyntheticGesture() {}

  virtual Result ForwardInputEvents(const base::TimeTicks& timestamp,
                                    SyntheticGestureTarget* target) OVERRIDE {
    step_count_++;
    if (step_count_ == num_steps_) {
      *finished_ = true;
      return SyntheticGesture::GESTURE_FINISHED;
    } else if (step_count_ > num_steps_) {
      *finished_ = true;
      // Return arbitrary failure.
      return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;
    }

    return SyntheticGesture::GESTURE_RUNNING;
  }

 protected:
  bool* finished_;
  int num_steps_;
  int step_count_;
};

class MockSyntheticGestureTarget : public SyntheticGestureTarget {
 public:
  MockSyntheticGestureTarget()
      : flush_requested_(false),
        pointer_assumed_stopped_time_ms_(kPointerAssumedStoppedTimeMs) {}
  virtual ~MockSyntheticGestureTarget() {}

  // SyntheticGestureTarget:
  virtual void DispatchInputEventToPlatform(
      const WebInputEvent& event) OVERRIDE {}

  virtual void SetNeedsFlush() OVERRIDE {
    flush_requested_ = true;
  }

  virtual SyntheticGestureParams::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const OVERRIDE {
    return SyntheticGestureParams::TOUCH_INPUT;
  }
  virtual bool SupportsSyntheticGestureSourceType(
      SyntheticGestureParams::GestureSourceType gesture_source_type)
      const OVERRIDE {
    return true;
  }

  virtual base::TimeDelta PointerAssumedStoppedTime() const OVERRIDE {
    return base::TimeDelta::FromMilliseconds(pointer_assumed_stopped_time_ms_);
  }

  void set_pointer_assumed_stopped_time_ms(int time_ms) {
    pointer_assumed_stopped_time_ms_ = time_ms;
  }

  virtual int GetTouchSlopInDips() const OVERRIDE {
    return kTouchSlopInDips;
  }

  bool flush_requested() const { return flush_requested_; }
  void ClearFlushRequest() { flush_requested_ = false; }

 private:
  bool flush_requested_;

  int pointer_assumed_stopped_time_ms_;
};

class MockSyntheticSmoothScrollGestureTarget
    : public MockSyntheticGestureTarget {
 public:
  MockSyntheticSmoothScrollGestureTarget() {}
  virtual ~MockSyntheticSmoothScrollGestureTarget() {}

  gfx::Vector2dF scroll_distance() const { return scroll_distance_; }

 protected:
  gfx::Vector2dF scroll_distance_;
};

class MockSyntheticSmoothScrollMouseTarget
    : public MockSyntheticSmoothScrollGestureTarget {
 public:
  MockSyntheticSmoothScrollMouseTarget() {}
  virtual ~MockSyntheticSmoothScrollMouseTarget() {}

  virtual void DispatchInputEventToPlatform(
      const WebInputEvent& event) OVERRIDE {
    ASSERT_EQ(event.type, WebInputEvent::MouseWheel);
    const WebMouseWheelEvent& mouse_wheel_event =
        static_cast<const WebMouseWheelEvent&>(event);
    scroll_distance_ -= gfx::Vector2dF(mouse_wheel_event.deltaX,
                                       mouse_wheel_event.deltaY);
  }
};

class MockSyntheticSmoothScrollTouchTarget
    : public MockSyntheticSmoothScrollGestureTarget {
 public:
  MockSyntheticSmoothScrollTouchTarget()
      : started_(false) {}
  virtual ~MockSyntheticSmoothScrollTouchTarget() {}

  virtual void DispatchInputEventToPlatform(
      const WebInputEvent& event) OVERRIDE {
    ASSERT_TRUE(WebInputEvent::isTouchEventType(event.type));
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    ASSERT_EQ(touch_event.touchesLength, 1U);

    if (!started_) {
      ASSERT_EQ(touch_event.type, WebInputEvent::TouchStart);
      anchor_.SetPoint(touch_event.touches[0].position.x,
                       touch_event.touches[0].position.y);
      started_ = true;
    } else {
      ASSERT_NE(touch_event.type, WebInputEvent::TouchStart);
      ASSERT_NE(touch_event.type, WebInputEvent::TouchCancel);
      // Ignore move events.

      if (touch_event.type == WebInputEvent::TouchEnd)
        scroll_distance_ =
            anchor_ - gfx::PointF(touch_event.touches[0].position.x,
                                  touch_event.touches[0].position.y);
    }
  }

 protected:
  gfx::Point anchor_;
  bool started_;
};

class MockSyntheticPinchTouchTarget : public MockSyntheticGestureTarget {
 public:
  enum ZoomDirection {
    ZOOM_DIRECTION_UNKNOWN,
    ZOOM_IN,
    ZOOM_OUT
  };

  MockSyntheticPinchTouchTarget()
      : total_num_pixels_covered_(0),
        last_pointer_distance_(0),
        zoom_direction_(ZOOM_DIRECTION_UNKNOWN),
        started_(false) {}
  virtual ~MockSyntheticPinchTouchTarget() {}

  virtual void DispatchInputEventToPlatform(
      const WebInputEvent& event) OVERRIDE {
    ASSERT_TRUE(WebInputEvent::isTouchEventType(event.type));
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    ASSERT_EQ(touch_event.touchesLength, 2U);

    if (!started_) {
      ASSERT_EQ(touch_event.type, WebInputEvent::TouchStart);

      start_0_ = gfx::PointF(touch_event.touches[0].position.x,
                             touch_event.touches[0].position.y);
      start_1_ = gfx::PointF(touch_event.touches[1].position.x,
                             touch_event.touches[1].position.y);
      last_pointer_distance_ = (start_0_ - start_1_).Length();

      started_ = true;
    } else {
      ASSERT_NE(touch_event.type, WebInputEvent::TouchStart);
      ASSERT_NE(touch_event.type, WebInputEvent::TouchCancel);

      gfx::PointF current_0 = gfx::PointF(touch_event.touches[0].position.x,
                                          touch_event.touches[0].position.y);
      gfx::PointF current_1 = gfx::PointF(touch_event.touches[1].position.x,
                                          touch_event.touches[1].position.y);

      total_num_pixels_covered_ =
          (current_0 - start_0_).Length() + (current_1 - start_1_).Length();
      float pointer_distance = (current_0 - current_1).Length();

      if (last_pointer_distance_ != pointer_distance) {
        if (zoom_direction_ == ZOOM_DIRECTION_UNKNOWN)
          zoom_direction_ =
              ComputeZoomDirection(last_pointer_distance_, pointer_distance);
        else
          EXPECT_EQ(
              zoom_direction_,
              ComputeZoomDirection(last_pointer_distance_, pointer_distance));
      }

      last_pointer_distance_ = pointer_distance;
    }
  }

  float total_num_pixels_covered() const { return total_num_pixels_covered_; }
  ZoomDirection zoom_direction() const { return zoom_direction_; }

 private:
  ZoomDirection ComputeZoomDirection(float last_pointer_distance,
                                     float current_pointer_distance) {
    DCHECK_NE(last_pointer_distance, current_pointer_distance);
    return last_pointer_distance < current_pointer_distance ? ZOOM_IN
                                                            : ZOOM_OUT;
  }

  float total_num_pixels_covered_;
  float last_pointer_distance_;
  ZoomDirection zoom_direction_;
  gfx::PointF start_0_;
  gfx::PointF start_1_;
  bool started_;
};

class MockSyntheticTapGestureTarget : public MockSyntheticGestureTarget {
 public:
  MockSyntheticTapGestureTarget() : state_(NOT_STARTED) {}
  virtual ~MockSyntheticTapGestureTarget() {}

  bool GestureFinished() const { return state_ == FINISHED; }
  gfx::PointF position() const { return position_; }
  base::TimeDelta GetDuration() const { return stop_time_ - start_time_; }

 protected:
  enum GestureState {
    NOT_STARTED,
    STARTED,
    FINISHED
  };

  // TODO(tdresser): clean up accesses to position_ once WebTouchPoint stores
  // its location as a WebFloatPoint. See crbug.com/336807.
  gfx::PointF position_;
  base::TimeDelta start_time_;
  base::TimeDelta stop_time_;
  GestureState state_;
};

class MockSyntheticTapTouchTarget : public MockSyntheticTapGestureTarget {
 public:
  MockSyntheticTapTouchTarget() {}
  virtual ~MockSyntheticTapTouchTarget() {}

  virtual void DispatchInputEventToPlatform(
        const WebInputEvent& event) OVERRIDE {
    ASSERT_TRUE(WebInputEvent::isTouchEventType(event.type));
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    ASSERT_EQ(touch_event.touchesLength, 1U);

    switch (state_) {
      case NOT_STARTED:
        EXPECT_EQ(touch_event.type, WebInputEvent::TouchStart);
        position_ = gfx::PointF(touch_event.touches[0].position.x,
                                touch_event.touches[0].position.y);
        start_time_ = base::TimeDelta::FromMilliseconds(
            static_cast<int64>(touch_event.timeStampSeconds * 1000));
        state_ = STARTED;
        break;
      case STARTED:
        EXPECT_EQ(touch_event.type, WebInputEvent::TouchEnd);
        EXPECT_EQ(position_, gfx::PointF(touch_event.touches[0].position.x,
                                         touch_event.touches[0].position.y));
        stop_time_ = base::TimeDelta::FromMilliseconds(
            static_cast<int64>(touch_event.timeStampSeconds * 1000));
        state_ = FINISHED;
        break;
      case FINISHED:
        EXPECT_FALSE(true);
        break;
    }
  }
};

class MockSyntheticTapMouseTarget : public MockSyntheticTapGestureTarget {
 public:
  MockSyntheticTapMouseTarget() {}
  virtual ~MockSyntheticTapMouseTarget() {}

  virtual void DispatchInputEventToPlatform(
        const WebInputEvent& event) OVERRIDE {
    ASSERT_TRUE(WebInputEvent::isMouseEventType(event.type));
    const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);

    switch (state_) {
      case NOT_STARTED:
        EXPECT_EQ(mouse_event.type, WebInputEvent::MouseDown);
        EXPECT_EQ(mouse_event.button, WebMouseEvent::ButtonLeft);
        EXPECT_EQ(mouse_event.clickCount, 1);
        position_ = gfx::Point(mouse_event.x, mouse_event.y);
        start_time_ = base::TimeDelta::FromMilliseconds(
            static_cast<int64>(mouse_event.timeStampSeconds * 1000));
        state_ = STARTED;
        break;
      case STARTED:
        EXPECT_EQ(mouse_event.type, WebInputEvent::MouseUp);
        EXPECT_EQ(mouse_event.button, WebMouseEvent::ButtonLeft);
        EXPECT_EQ(mouse_event.clickCount, 1);
        EXPECT_EQ(position_, gfx::Point(mouse_event.x, mouse_event.y));
        stop_time_ = base::TimeDelta::FromMilliseconds(
            static_cast<int64>(mouse_event.timeStampSeconds * 1000));
        state_ = FINISHED;
        break;
      case FINISHED:
        EXPECT_FALSE(true);
        break;
    }
  }
};

class SyntheticGestureControllerTest : public testing::Test {
 public:
  SyntheticGestureControllerTest() {}
  virtual ~SyntheticGestureControllerTest() {}

 protected:
  template<typename MockGestureTarget>
  void CreateControllerAndTarget() {
    target_ = new MockGestureTarget();
    controller_.reset(new SyntheticGestureController(
        scoped_ptr<SyntheticGestureTarget>(target_)));
  }

  virtual void SetUp() OVERRIDE {
    start_time_ = base::TimeTicks::Now();
    time_ = start_time_;
    num_success_ = 0;
    num_failure_ = 0;
  }

  virtual void TearDown() OVERRIDE {
    controller_.reset();
    target_ = NULL;
    time_ = base::TimeTicks();
  }

  void QueueSyntheticGesture(scoped_ptr<SyntheticGesture> gesture) {
    controller_->QueueSyntheticGesture(gesture.Pass(),
        base::Bind(&SyntheticGestureControllerTest::OnSyntheticGestureCompleted,
            base::Unretained(this)));
  }

  void FlushInputUntilComplete() {
    while (target_->flush_requested()) {
      target_->ClearFlushRequest();
      time_ += base::TimeDelta::FromMilliseconds(kFlushInputRateInMs);
      controller_->Flush(time_);
    }
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    DCHECK_NE(result, SyntheticGesture::GESTURE_RUNNING);
    if (result == SyntheticGesture::GESTURE_FINISHED)
      num_success_++;
    else
      num_failure_++;
  }

  base::TimeDelta GetTotalTime() const { return time_ - start_time_; }

  MockSyntheticGestureTarget* target_;
  scoped_ptr<SyntheticGestureController> controller_;
  base::TimeTicks start_time_;
  base::TimeTicks time_;
  int num_success_;
  int num_failure_;
};

TEST_F(SyntheticGestureControllerTest, SingleGesture) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished;
  scoped_ptr<MockSyntheticGesture> gesture(
      new MockSyntheticGesture(&finished, 3));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  EXPECT_TRUE(finished);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
}

TEST_F(SyntheticGestureControllerTest, GestureFailed) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished;
  scoped_ptr<MockSyntheticGesture> gesture(
      new MockSyntheticGesture(&finished, 0));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  EXPECT_TRUE(finished);
  EXPECT_EQ(1, num_failure_);
  EXPECT_EQ(0, num_success_);
}

TEST_F(SyntheticGestureControllerTest, SuccessiveGestures) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished_1, finished_2;
  scoped_ptr<MockSyntheticGesture> gesture_1(
      new MockSyntheticGesture(&finished_1, 2));
  scoped_ptr<MockSyntheticGesture> gesture_2(
      new MockSyntheticGesture(&finished_2, 4));

  // Queue first gesture and wait for it to finish
  QueueSyntheticGesture(gesture_1.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  EXPECT_TRUE(finished_1);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);

  // Queue second gesture.
  QueueSyntheticGesture(gesture_2.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  EXPECT_TRUE(finished_2);
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
}

TEST_F(SyntheticGestureControllerTest, TwoGesturesInFlight) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished_1, finished_2;
  scoped_ptr<MockSyntheticGesture> gesture_1(
      new MockSyntheticGesture(&finished_1, 2));
  scoped_ptr<MockSyntheticGesture> gesture_2(
      new MockSyntheticGesture(&finished_2, 4));

  QueueSyntheticGesture(gesture_1.PassAs<SyntheticGesture>());
  QueueSyntheticGesture(gesture_2.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  EXPECT_TRUE(finished_1);
  EXPECT_TRUE(finished_2);

  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
}

gfx::Vector2d AddTouchSlopToVector(const gfx::Vector2d& vector,
                                   SyntheticGestureTarget* target) {
  const int kTouchSlop = target->GetTouchSlopInDips();

  int x = vector.x();
  if (x > 0)
    x += kTouchSlop;
  else if (x < 0)
    x -= kTouchSlop;

  int y = vector.y();
  if (y > 0)
    y += kTouchSlop;
  else if (y < 0)
    y -= kTouchSlop;

  return gfx::Vector2d(x, y);
}

TEST_F(SyntheticGestureControllerTest, SmoothScrollGestureTouchVertical) {
  CreateControllerAndTarget<MockSyntheticSmoothScrollTouchTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.distance = gfx::Vector2d(0, 123);
  params.anchor.SetPoint(89, 32);

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticSmoothScrollGestureTarget* smooth_scroll_target =
      static_cast<MockSyntheticSmoothScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(AddTouchSlopToVector(params.distance, target_),
            smooth_scroll_target->scroll_distance());
}

TEST_F(SyntheticGestureControllerTest, SmoothScrollGestureTouchHorizontal) {
  CreateControllerAndTarget<MockSyntheticSmoothScrollTouchTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.distance = gfx::Vector2d(-234, 0);
  params.anchor.SetPoint(12, -23);

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticSmoothScrollGestureTarget* smooth_scroll_target =
      static_cast<MockSyntheticSmoothScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(AddTouchSlopToVector(params.distance, target_),
            smooth_scroll_target->scroll_distance());
}

void CheckIsWithinRange(float scroll_distance,
                        int target_distance,
                        SyntheticGestureTarget* target) {
  if (target_distance > 0) {
    EXPECT_LE(target_distance, scroll_distance);
    EXPECT_LE(scroll_distance, target_distance + target->GetTouchSlopInDips());
  } else {
    EXPECT_GE(target_distance, scroll_distance);
    EXPECT_GE(scroll_distance, target_distance - target->GetTouchSlopInDips());
  }
}

void CheckScrollDistanceIsWithinRange(const gfx::Vector2dF& scroll_distance,
                                      const gfx::Vector2d& target_distance,
                                      SyntheticGestureTarget* target) {
  CheckIsWithinRange(scroll_distance.x(), target_distance.x(), target);
  CheckIsWithinRange(scroll_distance.y(), target_distance.y(), target);
}

TEST_F(SyntheticGestureControllerTest, SmoothScrollGestureTouchDiagonal) {
  CreateControllerAndTarget<MockSyntheticSmoothScrollTouchTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.distance = gfx::Vector2d(413, -83);
  params.anchor.SetPoint(0, 7);

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticSmoothScrollGestureTarget* smooth_scroll_target =
      static_cast<MockSyntheticSmoothScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  CheckScrollDistanceIsWithinRange(
      smooth_scroll_target->scroll_distance(), params.distance, target_);
}

TEST_F(SyntheticGestureControllerTest, SmoothScrollGestureTouchLongStop) {
  CreateControllerAndTarget<MockSyntheticSmoothScrollTouchTarget>();

  // Create a smooth scroll with a short distance and set the pointer assumed
  // stopped time high, so that the stopping should dominate the time the
  // gesture is active.
  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.distance = gfx::Vector2d(21, -12);
  params.prevent_fling = true;
  params.anchor.SetPoint(-98, -23);

  target_->set_pointer_assumed_stopped_time_ms(543);

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticSmoothScrollGestureTarget* smooth_scroll_target =
      static_cast<MockSyntheticSmoothScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  CheckScrollDistanceIsWithinRange(
      smooth_scroll_target->scroll_distance(), params.distance, target_);
  EXPECT_GE(GetTotalTime(), target_->PointerAssumedStoppedTime());
}

TEST_F(SyntheticGestureControllerTest, SmoothScrollGestureTouchFling) {
  CreateControllerAndTarget<MockSyntheticSmoothScrollTouchTarget>();

  // Create a smooth scroll with a short distance and set the pointer assumed
  // stopped time high. Disable 'prevent_fling' and check that the gesture
  // finishes without waiting before it stops.
  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.distance = gfx::Vector2d(-43, 19);
  params.prevent_fling = false;
  params.anchor.SetPoint(-89, 78);

  target_->set_pointer_assumed_stopped_time_ms(543);

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticSmoothScrollGestureTarget* smooth_scroll_target =
      static_cast<MockSyntheticSmoothScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  CheckScrollDistanceIsWithinRange(
      smooth_scroll_target->scroll_distance(), params.distance, target_);
  EXPECT_LE(GetTotalTime(), target_->PointerAssumedStoppedTime());
}

TEST_F(SyntheticGestureControllerTest, SmoothScrollGestureTouchZeroDistance) {
  CreateControllerAndTarget<MockSyntheticSmoothScrollTouchTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.distance = gfx::Vector2d(0, 0);
  params.anchor.SetPoint(-32, 43);

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticSmoothScrollGestureTarget* smooth_scroll_target =
      static_cast<MockSyntheticSmoothScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(gfx::Vector2dF(0, 0), smooth_scroll_target->scroll_distance());
}

TEST_F(SyntheticGestureControllerTest, SmoothScrollGestureMouseVertical) {
  CreateControllerAndTarget<MockSyntheticSmoothScrollMouseTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.distance = gfx::Vector2d(0, -234);
  params.anchor.SetPoint(432, 89);

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticSmoothScrollGestureTarget* smooth_scroll_target =
      static_cast<MockSyntheticSmoothScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.distance, smooth_scroll_target->scroll_distance());
}

TEST_F(SyntheticGestureControllerTest, SmoothScrollGestureMouseHorizontal) {
  CreateControllerAndTarget<MockSyntheticSmoothScrollMouseTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.distance = gfx::Vector2d(345, 0);
  params.anchor.SetPoint(90, 12);

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticSmoothScrollGestureTarget* smooth_scroll_target =
      static_cast<MockSyntheticSmoothScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.distance, smooth_scroll_target->scroll_distance());
}

TEST_F(SyntheticGestureControllerTest, SmoothScrollGestureMouseDiagonal) {
  CreateControllerAndTarget<MockSyntheticSmoothScrollMouseTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.distance = gfx::Vector2d(-194, 303);
  params.anchor.SetPoint(90, 12);

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticSmoothScrollGestureTarget* smooth_scroll_target =
      static_cast<MockSyntheticSmoothScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.distance, smooth_scroll_target->scroll_distance());
}

TEST_F(SyntheticGestureControllerTest, PinchGestureTouchZoomIn) {
  CreateControllerAndTarget<MockSyntheticPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.zoom_in = true;
  params.total_num_pixels_covered = 345;
  params.anchor.SetPoint(54, 89);

  scoped_ptr<SyntheticPinchGesture> gesture(new SyntheticPinchGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticPinchTouchTarget::ZOOM_IN);
  EXPECT_EQ(params.total_num_pixels_covered + 2 * target_->GetTouchSlopInDips(),
            pinch_target->total_num_pixels_covered());
}

TEST_F(SyntheticGestureControllerTest, PinchGestureTouchZoomOut) {
  CreateControllerAndTarget<MockSyntheticPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.zoom_in = false;
  params.total_num_pixels_covered = 456;
  params.anchor.SetPoint(-12, 93);

  scoped_ptr<SyntheticPinchGesture> gesture(new SyntheticPinchGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticPinchTouchTarget::ZOOM_OUT);
  EXPECT_EQ(params.total_num_pixels_covered + 2 * target_->GetTouchSlopInDips(),
            pinch_target->total_num_pixels_covered());
}

TEST_F(SyntheticGestureControllerTest, PinchGestureTouchZeroPixelsCovered) {
  CreateControllerAndTarget<MockSyntheticPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.zoom_in = true;
  params.total_num_pixels_covered = 0;

  scoped_ptr<SyntheticPinchGesture> gesture(new SyntheticPinchGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticPinchTouchTarget::ZOOM_DIRECTION_UNKNOWN);
  EXPECT_EQ(0, pinch_target->total_num_pixels_covered());
}

TEST_F(SyntheticGestureControllerTest, TapGestureTouch) {
  CreateControllerAndTarget<MockSyntheticTapTouchTarget>();

  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.duration_ms = 123;
  params.position.SetPoint(87, -124);

  scoped_ptr<SyntheticTapGesture> gesture(new SyntheticTapGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticTapTouchTarget* tap_target =
      static_cast<MockSyntheticTapTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(tap_target->GestureFinished());
  EXPECT_EQ(tap_target->position(), params.position);
  EXPECT_EQ(tap_target->GetDuration().InMilliseconds(), params.duration_ms);
  EXPECT_GE(GetTotalTime(),
            base::TimeDelta::FromMilliseconds(params.duration_ms));
}

TEST_F(SyntheticGestureControllerTest, TapGestureMouse) {
  CreateControllerAndTarget<MockSyntheticTapMouseTarget>();

  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.duration_ms = 79;
  params.position.SetPoint(98, 123);

  scoped_ptr<SyntheticTapGesture> gesture(new SyntheticTapGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticTapMouseTarget* tap_target =
      static_cast<MockSyntheticTapMouseTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(tap_target->GestureFinished());
  EXPECT_EQ(tap_target->position(), params.position);
  EXPECT_EQ(tap_target->GetDuration().InMilliseconds(), params.duration_ms);
  EXPECT_GE(GetTotalTime(),
            base::TimeDelta::FromMilliseconds(params.duration_ms));
}

}  // namespace

}  // namespace content
