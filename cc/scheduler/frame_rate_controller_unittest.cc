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
    FakeFrameRateControllerClient() { reset(); }

    void reset() { m_vsyncTicked = false; }
    bool vsyncTicked() const { return m_vsyncTicked; }

    virtual void VSyncTick(bool throttled) OVERRIDE {
      m_vsyncTicked = !throttled;
    }

protected:
    bool m_vsyncTicked;
};


TEST(FrameRateControllerTest, TestFrameThrottling_ImmediateAck)
{
    FakeThread thread;
    FakeFrameRateControllerClient client;
    base::TimeDelta interval = base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond / 60);
    scoped_refptr<FakeDelayBasedTimeSource> timeSource = FakeDelayBasedTimeSource::create(interval, &thread);
    FrameRateController controller(timeSource);

    controller.SetClient(&client);
    controller.SetActive(true);

    base::TimeTicks elapsed; // Muck around with time a bit

    // Trigger one frame, make sure the vsync callback is called
    elapsed += base::TimeDelta::FromMilliseconds(thread.pendingDelayMs());
    timeSource->setNow(elapsed);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    // Tell the controller we drew
    controller.DidBeginFrame();

    // Tell the controller the frame ended 5ms later
    timeSource->setNow(timeSource->Now() + base::TimeDelta::FromMilliseconds(5));
    controller.DidFinishFrame();

    // Trigger another frame, make sure vsync runs again
    elapsed += base::TimeDelta::FromMilliseconds(thread.pendingDelayMs());
    EXPECT_TRUE(elapsed >= timeSource->Now()); // Sanity check that previous code didn't move time backward.
    timeSource->setNow(elapsed);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
}

TEST(FrameRateControllerTest, TestFrameThrottling_TwoFramesInFlight)
{
    FakeThread thread;
    FakeFrameRateControllerClient client;
    base::TimeDelta interval = base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond / 60);
    scoped_refptr<FakeDelayBasedTimeSource> timeSource = FakeDelayBasedTimeSource::create(interval, &thread);
    FrameRateController controller(timeSource);

    controller.SetClient(&client);
    controller.SetActive(true);
    controller.SetMaxFramesPending(2);

    base::TimeTicks elapsed; // Muck around with time a bit

    // Trigger one frame, make sure the vsync callback is called
    elapsed += base::TimeDelta::FromMilliseconds(thread.pendingDelayMs());
    timeSource->setNow(elapsed);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    // Tell the controller we drew
    controller.DidBeginFrame();

    // Trigger another frame, make sure vsync callback runs again
    elapsed += base::TimeDelta::FromMilliseconds(thread.pendingDelayMs());
    EXPECT_TRUE(elapsed >= timeSource->Now()); // Sanity check that previous code didn't move time backward.
    timeSource->setNow(elapsed);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    // Tell the controller we drew, again.
    controller.DidBeginFrame();

    // Trigger another frame. Since two frames are pending, we should not draw.
    elapsed += base::TimeDelta::FromMilliseconds(thread.pendingDelayMs());
    EXPECT_TRUE(elapsed >= timeSource->Now()); // Sanity check that previous code didn't move time backward.
    timeSource->setNow(elapsed);
    thread.runPendingTask();
    EXPECT_FALSE(client.vsyncTicked());

    // Tell the controller the first frame ended 5ms later
    timeSource->setNow(timeSource->Now() + base::TimeDelta::FromMilliseconds(5));
    controller.DidFinishFrame();

    // Tick should not have been called
    EXPECT_FALSE(client.vsyncTicked());

    // Trigger yet another frame. Since one frames is pending, another vsync callback should run.
    elapsed += base::TimeDelta::FromMilliseconds(thread.pendingDelayMs());
    EXPECT_TRUE(elapsed >= timeSource->Now()); // Sanity check that previous code didn't move time backward.
    timeSource->setNow(elapsed);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
}

TEST(FrameRateControllerTest, TestFrameThrottling_Unthrottled)
{
    FakeThread thread;
    FakeFrameRateControllerClient client;
    FrameRateController controller(&thread);

    controller.SetClient(&client);
    controller.SetMaxFramesPending(2);

    // setActive triggers 1st frame, make sure the vsync callback is called
    controller.SetActive(true);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    // Even if we don't call didBeginFrame, FrameRateController should
    // still attempt to vsync tick multiple times until it does result in
    // a didBeginFrame.
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    // didBeginFrame triggers 2nd frame, make sure the vsync callback is called
    controller.DidBeginFrame();
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    // didBeginFrame triggers 3rd frame (> maxFramesPending), make sure the vsync callback is NOT called
    controller.DidBeginFrame();
    thread.runPendingTask();
    EXPECT_FALSE(client.vsyncTicked());
    client.reset();

    // Make sure there is no pending task since we can't do anything until we receive a didFinishFrame anyway.
    EXPECT_FALSE(thread.hasPendingTask());

    // didFinishFrame triggers a frame, make sure the vsync callback is called
    controller.DidFinishFrame();
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
}

}  // namespace
}  // namespace cc
