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
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

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
    return base::TimeDelta::FromMilliseconds(16);
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

  int num_success() { return num_success_; }
  int num_failure() { return num_failure_; }

 private:
  int num_success_;
  int num_failure_;
};

class SyntheticGestureControllerNewTest : public testing::Test {
 public:
  SyntheticGestureControllerNewTest() {}
  virtual ~SyntheticGestureControllerNewTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    target_.reset(new MockSyntheticGestureTarget());
    controller_.reset(new SyntheticGestureControllerNew(target_.get()));
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
            target_->GetSyntheticGestureUpdateRate().InMilliseconds() *
            num_steps));
    base::MessageLoop::current()->Run();
  }

  base::MessageLoopForUI message_loop_;

  scoped_ptr<SyntheticGestureControllerNew> controller_;
  scoped_ptr<MockSyntheticGestureTarget> target_;
};

TEST_F(SyntheticGestureControllerNewTest, SingleGesture) {
  bool finished;
  scoped_ptr<MockSyntheticGesture> gesture(
      new MockSyntheticGesture(&finished, 3));
  controller_->QueueSyntheticGesture(gesture.PassAs<SyntheticGestureNew>());
  PostQuitMessageAndRun(5);

  EXPECT_TRUE(finished);
  EXPECT_EQ(1, target_->num_success());
  EXPECT_EQ(0, target_->num_failure());
}

TEST_F(SyntheticGestureControllerNewTest, GestureFailed) {
  bool finished;
  scoped_ptr<MockSyntheticGesture> gesture(
      new MockSyntheticGesture(&finished, 0));
  controller_->QueueSyntheticGesture(gesture.PassAs<SyntheticGestureNew>());
  PostQuitMessageAndRun(5);

  EXPECT_TRUE(finished);
  EXPECT_EQ(1, target_->num_failure());
  EXPECT_EQ(0, target_->num_success());
}

TEST_F(SyntheticGestureControllerNewTest, SuccessiveGestures) {
  bool finished_1, finished_2;
  scoped_ptr<MockSyntheticGesture> gesture_1(
      new MockSyntheticGesture(&finished_1, 2));
  scoped_ptr<MockSyntheticGesture> gesture_2(
      new MockSyntheticGesture(&finished_2, 4));

  // Queue first gesture and wait for it to finish
  controller_->QueueSyntheticGesture(gesture_1.PassAs<SyntheticGestureNew>());
  PostQuitMessageAndRun(4);

  EXPECT_TRUE(finished_1);
  EXPECT_EQ(1, target_->num_success());
  EXPECT_EQ(0, target_->num_failure());

  // Queue second gesture.
  controller_->QueueSyntheticGesture(gesture_2.PassAs<SyntheticGestureNew>());
  PostQuitMessageAndRun(6);

  EXPECT_TRUE(finished_2);
  EXPECT_EQ(2, target_->num_success());
  EXPECT_EQ(0, target_->num_failure());
}

TEST_F(SyntheticGestureControllerNewTest, TwoGesturesInFlight) {
  bool finished_1, finished_2;
  scoped_ptr<MockSyntheticGesture> gesture_1(
      new MockSyntheticGesture(&finished_1, 2));
  scoped_ptr<MockSyntheticGesture> gesture_2(
      new MockSyntheticGesture(&finished_2, 4));

  controller_->QueueSyntheticGesture(gesture_1.PassAs<SyntheticGestureNew>());
  controller_->QueueSyntheticGesture(gesture_2.PassAs<SyntheticGestureNew>());
  PostQuitMessageAndRun(10);

  EXPECT_TRUE(finished_1);
  EXPECT_TRUE(finished_2);

  EXPECT_EQ(2, target_->num_success());
  EXPECT_EQ(0, target_->num_failure());
}

TEST_F(SyntheticGestureControllerNewTest, SmoothScrollGesture) {
  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.distance = 100;

  scoped_ptr<SyntheticSmoothScrollGestureNew> gesture(
      new SyntheticSmoothScrollGestureNew(params));
  controller_->QueueSyntheticGesture(gesture.PassAs<SyntheticGestureNew>());
  PostQuitMessageAndRun(20);

  EXPECT_EQ(1, target_->num_success());
  EXPECT_EQ(0, target_->num_failure());
}

}  // namespace

}  // namespace content
