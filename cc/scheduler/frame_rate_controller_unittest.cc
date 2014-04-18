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

  virtual void FrameRateControllerTick(const BeginFrameArgs& args) OVERRIDE {
    frame_count_ += 1;
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

  // Trigger another frame, make sure BeginFrame runs again
  elapsed += task_runner->NextPendingTaskDelay();
  // Sanity check that previous code didn't move time backward.
  EXPECT_GE(elapsed, time_source->Now());
  time_source->SetNow(elapsed);
  task_runner->RunPendingTasks();
  EXPECT_TRUE(client.BeganFrame());
}

}  // namespace
}  // namespace cc
