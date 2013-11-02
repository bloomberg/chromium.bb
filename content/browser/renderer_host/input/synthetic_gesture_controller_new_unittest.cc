// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture_controller_new.h"
#include "content/browser/renderer_host/input/synthetic_gesture_new.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture_new.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/common/input/input_event.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

namespace {

const int kSyntheticGestureUpdateRate = 1;

class MockSyntheticGesture : public SyntheticGestureNew {
 public:
  MockSyntheticGesture(bool *finished, int num_steps)
      : finished_(finished),
        num_steps_(num_steps),
        step_count_(0) {
    *finished_ = false;
  }
  virtual ~MockSyntheticGesture() {}

  virtual Result ForwardInputEvents(const base::TimeDelta& interval,
                                    SyntheticGestureTarget* target) OVERRIDE {
    step_count_++;
    if (step_count_ == num_steps_) {
      *finished_ = true;
      return SyntheticGestureNew::GESTURE_FINISHED;
    }
    else if (step_count_ > num_steps_) {
      *finished_ = true;
      // Return arbitrary failure.
      return SyntheticGestureNew::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;
    }
    else
      return SyntheticGestureNew::GESTURE_RUNNING;
  }

 protected:
  bool* finished_;
  int num_steps_;
  int step_count_;
};

class MockSyntheticGestureTarget : public SyntheticGestureTarget {
 public:
  MockSyntheticGestureTarget() : num_success_(0), num_failure_(0) {}
  virtual ~MockSyntheticGestureTarget() {}

  virtual void QueueInputEventToPlatform(const InputEvent& event) OVERRIDE {}

  virtual void OnSyntheticGestureCompleted(
      SyntheticGestureNew::Result result) OVERRIDE {
    DCHECK_NE(result, SyntheticGestureNew::GESTURE_RUNNING);
    if (result == SyntheticGestureNew::GESTURE_FINISHED)
      num_success_++;
    else
      num_failure_++;
  }

  virtual base::TimeDelta GetSyntheticGestureUpdateRate() const OVERRIDE {
    return base::TimeDelta::FromMilliseconds(kSyntheticGestureUpdateRate);
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

  int num_success() const { return num_success_; }
  int num_failure() const { return num_failure_; }

 private:
  int num_success_;
  int num_failure_;
};

class MockSyntheticSmoothScrollMouseTarget : public MockSyntheticGestureTarget {
 public:
  MockSyntheticSmoothScrollMouseTarget() : scroll_distance_(0) {}
  virtual ~MockSyntheticSmoothScrollMouseTarget() {}

  virtual void QueueInputEventToPlatform(const InputEvent& event) OVERRIDE {
    const WebKit::WebInputEvent* web_event = event.web_event.get();
    DCHECK_EQ(web_event->type, WebKit::WebInputEvent::MouseWheel);
    const WebKit::WebMouseWheelEvent* mouse_wheel_event =
        static_cast<const WebKit::WebMouseWheelEvent*>(web_event);
    DCHECK_EQ(mouse_wheel_event->deltaX, 0);
    scroll_distance_ += mouse_wheel_event->deltaY;
  }

  float scroll_distance() const { return scroll_distance_; }

 private:
  float scroll_distance_;
};

class MockSyntheticSmoothScrollTouchTarget : public MockSyntheticGestureTarget {
 public:
  MockSyntheticSmoothScrollTouchTarget()
      : scroll_distance_(0), anchor_y_(0), started_(false) {}
  virtual ~MockSyntheticSmoothScrollTouchTarget() {}

  virtual void QueueInputEventToPlatform(const InputEvent& event) OVERRIDE {
    const WebKit::WebInputEvent* web_event = event.web_event.get();
    DCHECK(WebKit::WebInputEvent::isTouchEventType(web_event->type));
    const WebKit::WebTouchEvent* touch_event =
        static_cast<const WebKit::WebTouchEvent*>(web_event);
    DCHECK_EQ(touch_event->touchesLength, (unsigned int)1);

    if (!started_) {
      DCHECK_EQ(touch_event->type, WebKit::WebInputEvent::TouchStart);
      anchor_y_ = touch_event->touches[0].position.y;
      started_ = true;
    }
    else {
      DCHECK_NE(touch_event->type, WebKit::WebInputEvent::TouchStart);
      DCHECK_NE(touch_event->type, WebKit::WebInputEvent::TouchCancel);
      // Ignore move events.

      if (touch_event->type == WebKit::WebInputEvent::TouchEnd)
        scroll_distance_ = touch_event->touches[0].position.y - anchor_y_;
    }
  }

  float scroll_distance() const { return scroll_distance_; }

 private:
  float scroll_distance_;
  float anchor_y_;
  bool started_;
};

class SyntheticGestureControllerNewTest : public testing::Test {
 public:
  SyntheticGestureControllerNewTest() {}
  virtual ~SyntheticGestureControllerNewTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    target_.reset(new MockSyntheticGestureTarget());
    target_ptr_ = target_.get();
    controller_.reset(new SyntheticGestureControllerNew(
        target_.PassAs<SyntheticGestureTarget>()));
  }

  virtual void TearDown() OVERRIDE {
    controller_.reset();
    target_.reset();

    // Process all pending tasks to avoid leaks.
    base::MessageLoop::current()->RunUntilIdle();
  }

  void PostQuitMessageAndRun(int num_steps) {
    // Allow the message loop to process pending synthetic scrolls, then quit.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::MessageLoop::QuitClosure(),
        base::TimeDelta::FromMilliseconds(
            kSyntheticGestureUpdateRate * num_steps));
    base::MessageLoop::current()->Run();
  }

  base::MessageLoopForUI message_loop_;

  scoped_ptr<SyntheticGestureControllerNew> controller_;
  scoped_ptr<MockSyntheticGestureTarget> target_;
  const MockSyntheticGestureTarget* target_ptr_;
};

TEST_F(SyntheticGestureControllerNewTest, SingleGesture) {
  bool finished;
  scoped_ptr<MockSyntheticGesture> gesture(
      new MockSyntheticGesture(&finished, 3));
  controller_->QueueSyntheticGesture(gesture.PassAs<SyntheticGestureNew>());
  PostQuitMessageAndRun(30);

  EXPECT_TRUE(finished);
  EXPECT_EQ(1, target_ptr_->num_success());
  EXPECT_EQ(0, target_ptr_->num_failure());
}

TEST_F(SyntheticGestureControllerNewTest, GestureFailed) {
  bool finished;
  scoped_ptr<MockSyntheticGesture> gesture(
      new MockSyntheticGesture(&finished, 0));
  controller_->QueueSyntheticGesture(gesture.PassAs<SyntheticGestureNew>());
  PostQuitMessageAndRun(10);

  EXPECT_TRUE(finished);
  EXPECT_EQ(1, target_ptr_->num_failure());
  EXPECT_EQ(0, target_ptr_->num_success());
}

TEST_F(SyntheticGestureControllerNewTest, SuccessiveGestures) {
  bool finished_1, finished_2;
  scoped_ptr<MockSyntheticGesture> gesture_1(
      new MockSyntheticGesture(&finished_1, 2));
  scoped_ptr<MockSyntheticGesture> gesture_2(
      new MockSyntheticGesture(&finished_2, 4));

  // Queue first gesture and wait for it to finish
  controller_->QueueSyntheticGesture(gesture_1.PassAs<SyntheticGestureNew>());
  PostQuitMessageAndRun(30);

  EXPECT_TRUE(finished_1);
  EXPECT_EQ(1, target_ptr_->num_success());
  EXPECT_EQ(0, target_ptr_->num_failure());

  // Queue second gesture.
  controller_->QueueSyntheticGesture(gesture_2.PassAs<SyntheticGestureNew>());
  PostQuitMessageAndRun(30);

  EXPECT_TRUE(finished_2);
  EXPECT_EQ(2, target_ptr_->num_success());
  EXPECT_EQ(0, target_ptr_->num_failure());
}

// TODO(dominikg): Started flaking around r231819 http://crbug.com/314272
TEST_F(SyntheticGestureControllerNewTest, DISABLED_TwoGesturesInFlight) {
  bool finished_1, finished_2;
  scoped_ptr<MockSyntheticGesture> gesture_1(
      new MockSyntheticGesture(&finished_1, 2));
  scoped_ptr<MockSyntheticGesture> gesture_2(
      new MockSyntheticGesture(&finished_2, 4));

  controller_->QueueSyntheticGesture(gesture_1.PassAs<SyntheticGestureNew>());
  controller_->QueueSyntheticGesture(gesture_2.PassAs<SyntheticGestureNew>());
  PostQuitMessageAndRun(30);

  EXPECT_TRUE(finished_1);
  EXPECT_TRUE(finished_2);

  EXPECT_EQ(2, target_ptr_->num_success());
  EXPECT_EQ(0, target_ptr_->num_failure());
}

TEST_F(SyntheticGestureControllerNewTest, SmoothScrollGestureTouch) {
  scoped_ptr<MockSyntheticSmoothScrollTouchTarget> smooth_scroll_touch_target(
      new MockSyntheticSmoothScrollTouchTarget);
  const MockSyntheticSmoothScrollTouchTarget* smooth_scroll_touch_target_ptr =
      smooth_scroll_touch_target.get();
  controller_.reset(new SyntheticGestureControllerNew(
      smooth_scroll_touch_target.PassAs<SyntheticGestureTarget>()));

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.distance = 6;

  scoped_ptr<SyntheticSmoothScrollGestureNew> gesture(
      new SyntheticSmoothScrollGestureNew(params));
  controller_->QueueSyntheticGesture(gesture.PassAs<SyntheticGestureNew>());
  PostQuitMessageAndRun(30);

  EXPECT_EQ(1, smooth_scroll_touch_target_ptr->num_success());
  EXPECT_EQ(0, smooth_scroll_touch_target_ptr->num_failure());
  EXPECT_LE(params.distance, smooth_scroll_touch_target_ptr->scroll_distance());
}

TEST_F(SyntheticGestureControllerNewTest, SmoothScrollGestureMouse) {
  scoped_ptr<MockSyntheticSmoothScrollMouseTarget> smooth_scroll_mouse_target(
      new MockSyntheticSmoothScrollMouseTarget);
  const MockSyntheticSmoothScrollMouseTarget* smooth_scroll_mouse_target_ptr =
      smooth_scroll_mouse_target.get();
  controller_.reset(new SyntheticGestureControllerNew(
      smooth_scroll_mouse_target.PassAs<SyntheticGestureTarget>()));

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.distance = -6;

  scoped_ptr<SyntheticSmoothScrollGestureNew> gesture(
      new SyntheticSmoothScrollGestureNew(params));
  controller_->QueueSyntheticGesture(gesture.PassAs<SyntheticGestureNew>());
  PostQuitMessageAndRun(30);

  EXPECT_EQ(1, smooth_scroll_mouse_target_ptr->num_success());
  EXPECT_EQ(0, smooth_scroll_mouse_target_ptr->num_failure());
  EXPECT_GE(params.distance, smooth_scroll_mouse_target_ptr->scroll_distance());
}

}  // namespace

}  // namespace content
