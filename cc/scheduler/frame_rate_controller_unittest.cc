// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/frame_rate_controller.h"

#include "base/test/test_simple_task_runner.h"
#include "cc/test/scheduler_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class FakeFrameRateControllerClient : public FrameRateControllerClient {
 public:
  FakeFrameRateControllerClient() { Reset(); }

  void Reset() { frame_count_ = 0; }
  bool BeganFrame() const { return frame_count_ > 0; }
  int frame_count() const { return frame_count_; }

  virtual void FrameRateControllerTick(
      bool throttled, const BeginFrameArgs& args) OVERRIDE {
    frame_count_ += throttled ? 0 : 1;
  }

 protected:
  int frame_count_;
};

TEST(FrameRateControllerTest, TestFrameThrottling_ImmediateAck) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeFrameRateControllerClient client;
  base::TimeDelta interval = base::TimeDelta::FromMicroseconds(
      base::Time::kMicrosecondsPerSecond / 60);
  scoped_refptr<FakeDelayBasedTimeSource> time_source =
      FakeDelayBasedTimeSource::Create(interval, task_runner.get());
  FrameRateController controller(time_source);

  controller.SetClient(&client);
  controller.SetActive(true);

  base::TimeTicks elapsed;  // Muck around with time a bit

  // Trigger one frame, make sure the BeginFrame callback is called
  elapsed += task_runner->NextPendingTaskDelay();
  time_source->SetNow(elapsed);
  task_runner->RunPendingTasks();
  EXPECT_TRUE(client.BeganFrame());
  client.Reset();

  // Tell the controller we drew
  controller.DidSwapBuffers();

  // Tell the controller the frame ended 5ms later
  time_source->SetNow(time_source->Now() +
                      base::TimeDelta::FromMilliseconds(5));
  controller.DidSwapBuffersComplete();

  // Trigger another frame, make sure BeginFrame runs again
  elapsed += task_runner->NextPendingTaskDelay();
  // Sanity check that previous code didn't move time backward.
  EXPECT_GE(elapsed, time_source->Now());
  time_source->SetNow(elapsed);
  task_runner->RunPendingTasks();
  EXPECT_TRUE(client.BeganFrame());
}

TEST(FrameRateControllerTest, TestFrameThrottling_TwoFramesInFlight) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeFrameRateControllerClient client;
  base::TimeDelta interval = base::TimeDelta::FromMicroseconds(
      base::Time::kMicrosecondsPerSecond / 60);
  scoped_refptr<FakeDelayBasedTimeSource> time_source =
      FakeDelayBasedTimeSource::Create(interval, task_runner.get());
  FrameRateController controller(time_source);

  controller.SetClient(&client);
  controller.SetActive(true);
  controller.SetMaxSwapsPending(2);

  base::TimeTicks elapsed;  // Muck around with time a bit

  // Trigger one frame, make sure the BeginFrame callback is called
  elapsed += task_runner->NextPendingTaskDelay();
  time_source->SetNow(elapsed);
  task_runner->RunPendingTasks();
  EXPECT_TRUE(client.BeganFrame());
  client.Reset();

  // Tell the controller we drew
  controller.DidSwapBuffers();

  // Trigger another frame, make sure BeginFrame callback runs again
  elapsed += task_runner->NextPendingTaskDelay();
  // Sanity check that previous code didn't move time backward.
  EXPECT_GE(elapsed, time_source->Now());
  time_source->SetNow(elapsed);
  task_runner->RunPendingTasks();
  EXPECT_TRUE(client.BeganFrame());
  client.Reset();

  // Tell the controller we drew, again.
  controller.DidSwapBuffers();

  // Trigger another frame. Since two frames are pending, we should not draw.
  elapsed += task_runner->NextPendingTaskDelay();
  // Sanity check that previous code didn't move time backward.
  EXPECT_GE(elapsed, time_source->Now());
  time_source->SetNow(elapsed);
  task_runner->RunPendingTasks();
  EXPECT_FALSE(client.BeganFrame());

  // Tell the controller the first frame ended 5ms later
  time_source->SetNow(time_source->Now() +
                      base::TimeDelta::FromMilliseconds(5));
  controller.DidSwapBuffersComplete();

  // Tick should not have been called
  EXPECT_FALSE(client.BeganFrame());

  // Trigger yet another frame. Since one frames is pending, another
  // BeginFrame callback should run.
  elapsed += task_runner->NextPendingTaskDelay();
  // Sanity check that previous code didn't move time backward.
  EXPECT_GE(elapsed, time_source->Now());
  time_source->SetNow(elapsed);
  task_runner->RunPendingTasks();
  EXPECT_TRUE(client.BeganFrame());
}

TEST(FrameRateControllerTest, TestFrameThrottling_Unthrottled) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeFrameRateControllerClient client;
  FrameRateController controller(task_runner.get());

  controller.SetClient(&client);
  controller.SetMaxSwapsPending(2);

  // SetActive triggers 1st frame, make sure the BeginFrame callback
  // is called
  controller.SetActive(true);
  task_runner->RunPendingTasks();
  EXPECT_TRUE(client.BeganFrame());
  client.Reset();

  // Even if we don't call DidSwapBuffers, FrameRateController should
  // still attempt to tick multiple times until it does result in
  // a DidSwapBuffers.
  task_runner->RunPendingTasks();
  EXPECT_TRUE(client.BeganFrame());
  client.Reset();

  task_runner->RunPendingTasks();
  EXPECT_TRUE(client.BeganFrame());
  client.Reset();

  // DidSwapBuffers triggers 2nd frame, make sure the BeginFrame callback is
  // called
  controller.DidSwapBuffers();
  task_runner->RunPendingTasks();
  EXPECT_TRUE(client.BeganFrame());
  client.Reset();

  // DidSwapBuffers triggers 3rd frame (> max_frames_pending),
  // make sure the BeginFrame callback is NOT called
  controller.DidSwapBuffers();
  task_runner->RunPendingTasks();
  EXPECT_FALSE(client.BeganFrame());
  client.Reset();

  // Make sure there is no pending task since we can't do anything until we
  // receive a DidSwapBuffersComplete anyway.
  EXPECT_FALSE(task_runner->HasPendingTask());

  // DidSwapBuffersComplete triggers a frame, make sure the BeginFrame
  // callback is called
  controller.DidSwapBuffersComplete();
  task_runner->RunPendingTasks();
  EXPECT_TRUE(client.BeganFrame());
}

TEST(FrameRateControllerTest, TestFrameThrottling_NoDoubleTicking) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeFrameRateControllerClient client;
  FrameRateController controller(task_runner.get());
  controller.SetClient(&client);

  // SetActive triggers 1st frame and queues another tick task since the time
  // source isn't throttling.
  controller.SetActive(true);
  task_runner->RunPendingTasks();
  EXPECT_TRUE(client.BeganFrame());
  client.Reset();
  EXPECT_TRUE(task_runner->HasPendingTask());

  // Simulate a frame swap. This shouldn't queue a second tick task.
  controller.DidSwapBuffers();
  controller.DidSwapBuffersComplete();

  // The client should only be ticked once.
  task_runner->RunPendingTasks();
  EXPECT_EQ(1, client.frame_count());
}

}  // namespace
}  // namespace cc
