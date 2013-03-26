// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/delay_based_time_source.h"

#include "base/basictypes.h"
#include "cc/base/thread.h"
#include "cc/test/scheduler_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

base::TimeDelta Interval() {
  return base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond /
                                           60);
}

TEST(DelayBasedTimeSourceTest, TaskPostedAndTickCalled) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::Create(Interval(), &thread);
  timer->SetClient(&client);

  timer->SetActive(true);
  EXPECT_TRUE(timer->Active());
  EXPECT_TRUE(thread.HasPendingTask());

  timer->SetNow(timer->Now() + base::TimeDelta::FromMilliseconds(16));
  thread.RunPendingTask();
  EXPECT_TRUE(timer->Active());
  EXPECT_TRUE(client.TickCalled());
}

TEST(DelayBasedTimeSource, TickNotCalledWithTaskPosted) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::Create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  EXPECT_TRUE(thread.HasPendingTask());
  timer->SetActive(false);
  thread.RunPendingTask();
  EXPECT_FALSE(client.TickCalled());
}

TEST(DelayBasedTimeSource, StartTwiceEnqueuesOneTask) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::Create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  EXPECT_TRUE(thread.HasPendingTask());
  thread.Reset();
  timer->SetActive(true);
  EXPECT_FALSE(thread.HasPendingTask());
}

TEST(DelayBasedTimeSource, StartWhenRunningDoesntTick) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::Create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  thread.RunPendingTask();
  thread.Reset();
  timer->SetActive(true);
  EXPECT_FALSE(thread.HasPendingTask());
}

// At 60Hz, when the tick returns at exactly the requested next time, make sure
// a 16ms next delay is posted.
TEST(DelayBasedTimeSource, NextDelaySaneWhenExactlyOnRequestedTime) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::Create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  // Run the first task, as that activates the timer and picks up a timebase.
  thread.RunPendingTask();

  EXPECT_EQ(16, thread.PendingDelayMs());

  timer->SetNow(timer->Now() + Interval());
  thread.RunPendingTask();

  EXPECT_EQ(16, thread.PendingDelayMs());
}

// At 60Hz, when the tick returns at slightly after the requested next time,
// make sure a 16ms next delay is posted.
TEST(DelayBasedTimeSource, NextDelaySaneWhenSlightlyAfterRequestedTime) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::Create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  // Run the first task, as that activates the timer and picks up a timebase.
  thread.RunPendingTask();

  EXPECT_EQ(16, thread.PendingDelayMs());

  timer->SetNow(timer->Now() + Interval() +
                base::TimeDelta::FromMicroseconds(1));
  thread.RunPendingTask();

  EXPECT_EQ(16, thread.PendingDelayMs());
}

// At 60Hz, when the tick returns at exactly 2*interval after the requested next
// time, make sure a 16ms next delay is posted.
TEST(DelayBasedTimeSource, NextDelaySaneWhenExactlyTwiceAfterRequestedTime) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::Create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  // Run the first task, as that activates the timer and picks up a timebase.
  thread.RunPendingTask();

  EXPECT_EQ(16, thread.PendingDelayMs());

  timer->SetNow(timer->Now() + 2 * Interval());
  thread.RunPendingTask();

  EXPECT_EQ(16, thread.PendingDelayMs());
}

// At 60Hz, when the tick returns at 2*interval and a bit after the requested
// next time, make sure a 16ms next delay is posted.
TEST(DelayBasedTimeSource, NextDelaySaneWhenSlightlyAfterTwiceRequestedTime) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::Create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  // Run the first task, as that activates the timer and picks up a timebase.
  thread.RunPendingTask();

  EXPECT_EQ(16, thread.PendingDelayMs());

  timer->SetNow(timer->Now() + 2 * Interval() +
                base::TimeDelta::FromMicroseconds(1));
  thread.RunPendingTask();

  EXPECT_EQ(16, thread.PendingDelayMs());
}

// At 60Hz, when the tick returns halfway to the next frame time, make sure
// a correct next delay value is posted.
TEST(DelayBasedTimeSource, NextDelaySaneWhenHalfAfterRequestedTime) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::Create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  // Run the first task, as that activates the timer and picks up a timebase.
  thread.RunPendingTask();

  EXPECT_EQ(16, thread.PendingDelayMs());

  timer->SetNow(timer->Now() + Interval() +
                base::TimeDelta::FromMilliseconds(8));
  thread.RunPendingTask();

  EXPECT_EQ(8, thread.PendingDelayMs());
}

// If the timebase and interval are updated with a jittery source, we want to
// make sure we do not double tick.
TEST(DelayBasedTimeSource, SaneHandlingOfJitteryTimebase) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::Create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  // Run the first task, as that activates the timer and picks up a timebase.
  thread.RunPendingTask();

  EXPECT_EQ(16, thread.PendingDelayMs());

  // Jitter timebase ~1ms late
  timer->SetNow(timer->Now() + Interval());
  timer->SetTimebaseAndInterval(
      timer->Now() + base::TimeDelta::FromMilliseconds(1), Interval());
  thread.RunPendingTask();

  // Without double tick prevention, PendingDelayMs would be 1.
  EXPECT_EQ(17, thread.PendingDelayMs());

  // Jitter timebase ~1ms early
  timer->SetNow(timer->Now() + Interval());
  timer->SetTimebaseAndInterval(
      timer->Now() - base::TimeDelta::FromMilliseconds(1), Interval());
  thread.RunPendingTask();

  EXPECT_EQ(15, thread.PendingDelayMs());
}

TEST(DelayBasedTimeSource, HandlesSignificantTimebaseChangesImmediately) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::Create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  // Run the first task, as that activates the timer and picks up a timebase.
  thread.RunPendingTask();

  EXPECT_EQ(16, thread.PendingDelayMs());

  // Tick, then shift timebase by +7ms.
  timer->SetNow(timer->Now() + Interval());
  thread.RunPendingTask();

  EXPECT_EQ(16, thread.PendingDelayMs());

  client.Reset();
  thread.RunPendingTaskOnOverwrite(true);
  base::TimeDelta jitter = base::TimeDelta::FromMilliseconds(7) +
                           base::TimeDelta::FromMicroseconds(1);
  timer->SetTimebaseAndInterval(timer->Now() + jitter, Interval());
  thread.RunPendingTaskOnOverwrite(false);

  EXPECT_FALSE(client.TickCalled());  // Make sure pending tasks were canceled.
  EXPECT_EQ(7, thread.PendingDelayMs());

  // Tick, then shift timebase by -7ms.
  timer->SetNow(timer->Now() + jitter);
  thread.RunPendingTask();

  EXPECT_EQ(16, thread.PendingDelayMs());

  client.Reset();
  thread.RunPendingTaskOnOverwrite(true);
  timer->SetTimebaseAndInterval(base::TimeTicks() + Interval(), Interval());
  thread.RunPendingTaskOnOverwrite(false);

  EXPECT_FALSE(client.TickCalled());  // Make sure pending tasks were canceled.
  EXPECT_EQ(16 - 7, thread.PendingDelayMs());
}

TEST(DelayBasedTimeSource, HanldlesSignificantIntervalChangesImmediately) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::Create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  // Run the first task, as that activates the timer and picks up a timebase.
  thread.RunPendingTask();

  EXPECT_EQ(16, thread.PendingDelayMs());

  // Tick, then double the interval.
  timer->SetNow(timer->Now() + Interval());
  thread.RunPendingTask();

  EXPECT_EQ(16, thread.PendingDelayMs());

  client.Reset();
  thread.RunPendingTaskOnOverwrite(true);
  timer->SetTimebaseAndInterval(base::TimeTicks() + Interval(), Interval() * 2);
  thread.RunPendingTaskOnOverwrite(false);

  EXPECT_FALSE(client.TickCalled());  // Make sure pending tasks were canceled.
  EXPECT_EQ(33, thread.PendingDelayMs());

  // Tick, then halve the interval.
  timer->SetNow(timer->Now() + Interval() * 2);
  thread.RunPendingTask();

  EXPECT_EQ(33, thread.PendingDelayMs());

  client.Reset();
  thread.RunPendingTaskOnOverwrite(true);
  timer->SetTimebaseAndInterval(base::TimeTicks() + Interval() * 3, Interval());
  thread.RunPendingTaskOnOverwrite(false);

  EXPECT_FALSE(client.TickCalled());  // Make sure pending tasks were canceled.
  EXPECT_EQ(16, thread.PendingDelayMs());
}

TEST(DelayBasedTimeSourceTest, AchievesTargetRateWithNoNoise) {
  int num_iterations = 10;

  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::Create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);

  double total_frame_time = 0.0;
  for (int i = 0; i < num_iterations; ++i) {
    int64 delay_ms = thread.PendingDelayMs();

    // accumulate the "delay"
    total_frame_time += delay_ms / 1000.0;

    // Run the callback exactly when asked
    timer->SetNow(timer->Now() + base::TimeDelta::FromMilliseconds(delay_ms));
    thread.RunPendingTask();
  }
  double average_interval =
      total_frame_time / static_cast<double>(num_iterations);
  EXPECT_NEAR(1.0 / 60.0, average_interval, 0.1);
}

TEST(DelayBasedTimeSource, TestDeactivateWhilePending) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::Create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);  // Should post a task.
  timer->SetActive(false);
  timer = NULL;
  thread.RunPendingTask();  // Should run the posted task without crashing.
}

TEST(DelayBasedTimeSource, TestDeactivateAndReactivateBeforeNextTickTime) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::Create(Interval(), &thread);
  timer->SetClient(&client);

  // Should run the activate task, and pick up a new timebase.
  timer->SetActive(true);
  thread.RunPendingTask();

  // Stop the timer
  timer->SetActive(false);

  // Task will be pending anyway, run it
  thread.RunPendingTask();

  // Start the timer again, but before the next tick time the timer previously
  // planned on using. That same tick time should still be targeted.
  timer->SetNow(timer->Now() + base::TimeDelta::FromMilliseconds(4));
  timer->SetActive(true);
  EXPECT_EQ(12, thread.PendingDelayMs());
}

TEST(DelayBasedTimeSource, TestDeactivateAndReactivateAfterNextTickTime) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::Create(Interval(), &thread);
  timer->SetClient(&client);

  // Should run the activate task, and pick up a new timebase.
  timer->SetActive(true);
  thread.RunPendingTask();

  // Stop the timer.
  timer->SetActive(false);

  // Task will be pending anyway, run it.
  thread.RunPendingTask();

  // Start the timer again, but before the next tick time the timer previously
  // planned on using. That same tick time should still be targeted.
  timer->SetNow(timer->Now() + base::TimeDelta::FromMilliseconds(20));
  timer->SetActive(true);
  EXPECT_EQ(13, thread.PendingDelayMs());
}

}  // namespace
}  // namespace cc
