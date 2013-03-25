// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/frame_rate_controller.h"

#include "cc/test/scheduler_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class FakeFrameRateControllerClient : public cc::FrameRateControllerClient {
 public:
  FakeFrameRateControllerClient() { Reset(); }

  void Reset() { vsync_ticked_ = false; }
  bool VSyncTicked() const { return vsync_ticked_; }

  virtual void VSyncTick(bool throttled) OVERRIDE {
    vsync_ticked_ = !throttled;
  }

 protected:
  bool vsync_ticked_;
};

TEST(FrameRateControllerTest, TestFrameThrottling_ImmediateAck) {
  FakeThread thread;
  FakeFrameRateControllerClient client;
  base::TimeDelta interval = base::TimeDelta::FromMicroseconds(
      base::Time::kMicrosecondsPerSecond / 60);
  scoped_refptr<FakeDelayBasedTimeSource> time_source =
      FakeDelayBasedTimeSource::Create(interval, &thread);
  FrameRateController controller(time_source);

  controller.SetClient(&client);
  controller.SetActive(true);

  base::TimeTicks elapsed;  // Muck around with time a bit

  // Trigger one frame, make sure the vsync callback is called
  elapsed += base::TimeDelta::FromMilliseconds(thread.PendingDelayMs());
  time_source->SetNow(elapsed);
  thread.RunPendingTask();
  EXPECT_TRUE(client.VSyncTicked());
  client.Reset();

  // Tell the controller we drew
  controller.DidBeginFrame();

  // Tell the controller the frame ended 5ms later
  time_source->SetNow(time_source->Now() +
                      base::TimeDelta::FromMilliseconds(5));
  controller.DidFinishFrame();

  // Trigger another frame, make sure vsync runs again
  elapsed += base::TimeDelta::FromMilliseconds(thread.PendingDelayMs());
  // Sanity check that previous code didn't move time backward.
  EXPECT_GE(elapsed, time_source->Now());
  time_source->SetNow(elapsed);
  thread.RunPendingTask();
  EXPECT_TRUE(client.VSyncTicked());
}

TEST(FrameRateControllerTest, TestFrameThrottling_TwoFramesInFlight) {
  FakeThread thread;
  FakeFrameRateControllerClient client;
  base::TimeDelta interval = base::TimeDelta::FromMicroseconds(
      base::Time::kMicrosecondsPerSecond / 60);
  scoped_refptr<FakeDelayBasedTimeSource> time_source =
      FakeDelayBasedTimeSource::Create(interval, &thread);
  FrameRateController controller(time_source);

  controller.SetClient(&client);
  controller.SetActive(true);
  controller.SetMaxFramesPending(2);

  base::TimeTicks elapsed;  // Muck around with time a bit

  // Trigger one frame, make sure the vsync callback is called
  elapsed += base::TimeDelta::FromMilliseconds(thread.PendingDelayMs());
  time_source->SetNow(elapsed);
  thread.RunPendingTask();
  EXPECT_TRUE(client.VSyncTicked());
  client.Reset();

  // Tell the controller we drew
  controller.DidBeginFrame();

  // Trigger another frame, make sure vsync callback runs again
  elapsed += base::TimeDelta::FromMilliseconds(thread.PendingDelayMs());
  // Sanity check that previous code didn't move time backward.
  EXPECT_GE(elapsed, time_source->Now());
  time_source->SetNow(elapsed);
  thread.RunPendingTask();
  EXPECT_TRUE(client.VSyncTicked());
  client.Reset();

  // Tell the controller we drew, again.
  controller.DidBeginFrame();

  // Trigger another frame. Since two frames are pending, we should not draw.
  elapsed += base::TimeDelta::FromMilliseconds(thread.PendingDelayMs());
  // Sanity check that previous code didn't move time backward.
  EXPECT_GE(elapsed, time_source->Now());
  time_source->SetNow(elapsed);
  thread.RunPendingTask();
  EXPECT_FALSE(client.VSyncTicked());

  // Tell the controller the first frame ended 5ms later
  time_source->SetNow(time_source->Now() +
                      base::TimeDelta::FromMilliseconds(5));
  controller.DidFinishFrame();

  // Tick should not have been called
  EXPECT_FALSE(client.VSyncTicked());

  // Trigger yet another frame. Since one frames is pending, another vsync
  // callback should run.
  elapsed += base::TimeDelta::FromMilliseconds(thread.PendingDelayMs());
  // Sanity check that previous code didn't move time backward.
  EXPECT_GE(elapsed, time_source->Now());
  time_source->SetNow(elapsed);
  thread.RunPendingTask();
  EXPECT_TRUE(client.VSyncTicked());
}

TEST(FrameRateControllerTest, TestFrameThrottling_Unthrottled) {
  FakeThread thread;
  FakeFrameRateControllerClient client;
  FrameRateController controller(&thread);

  controller.SetClient(&client);
  controller.SetMaxFramesPending(2);

  // SetActive triggers 1st frame, make sure the vsync callback is called
  controller.SetActive(true);
  thread.RunPendingTask();
  EXPECT_TRUE(client.VSyncTicked());
  client.Reset();

  // Even if we don't call DidBeginFrame, FrameRateController should
  // still attempt to vsync tick multiple times until it does result in
  // a DidBeginFrame.
  thread.RunPendingTask();
  EXPECT_TRUE(client.VSyncTicked());
  client.Reset();

  thread.RunPendingTask();
  EXPECT_TRUE(client.VSyncTicked());
  client.Reset();

  // DidBeginFrame triggers 2nd frame, make sure the vsync callback is called
  controller.DidBeginFrame();
  thread.RunPendingTask();
  EXPECT_TRUE(client.VSyncTicked());
  client.Reset();

  // DidBeginFrame triggers 3rd frame (> max_frames_pending),
  // make sure the vsync callback is NOT called
  controller.DidBeginFrame();
  thread.RunPendingTask();
  EXPECT_FALSE(client.VSyncTicked());
  client.Reset();

  // Make sure there is no pending task since we can't do anything until we
  // receive a DidFinishFrame anyway.
  EXPECT_FALSE(thread.HasPendingTask());

  // DidFinishFrame triggers a frame, make sure the vsync callback is called
  controller.DidFinishFrame();
  thread.RunPendingTask();
  EXPECT_TRUE(client.VSyncTicked());
}

}  // namespace
}  // namespace cc
