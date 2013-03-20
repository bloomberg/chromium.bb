// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/delay_based_time_source.h"

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
      FakeDelayBasedTimeSource::create(Interval(), &thread);
  timer->SetClient(&client);

  timer->SetActive(true);
  EXPECT_TRUE(timer->Active());
  EXPECT_TRUE(thread.hasPendingTask());

  timer->setNow(timer->Now() + base::TimeDelta::FromMilliseconds(16));
  thread.runPendingTask();
  EXPECT_TRUE(timer->Active());
  EXPECT_TRUE(client.tickCalled());
}

TEST(DelayBasedTimeSource, TickNotCalledWithTaskPosted) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  EXPECT_TRUE(thread.hasPendingTask());
  timer->SetActive(false);
  thread.runPendingTask();
  EXPECT_FALSE(client.tickCalled());
}

TEST(DelayBasedTimeSource, StartTwiceEnqueuesOneTask) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  EXPECT_TRUE(thread.hasPendingTask());
  thread.reset();
  timer->SetActive(true);
  EXPECT_FALSE(thread.hasPendingTask());
}

TEST(DelayBasedTimeSource, StartWhenRunningDoesntTick) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  thread.runPendingTask();
  thread.reset();
  timer->SetActive(true);
  EXPECT_FALSE(thread.hasPendingTask());
}

// At 60Hz, when the tick returns at exactly the requested next time, make sure
// a 16ms next delay is posted.
TEST(DelayBasedTimeSource, NextDelaySaneWhenExactlyOnRequestedTime) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  // Run the first task, as that activates the timer and picks up a timebase.
  thread.runPendingTask();

  EXPECT_EQ(16, thread.pendingDelayMs());

  timer->setNow(timer->Now() + Interval());
  thread.runPendingTask();

  EXPECT_EQ(16, thread.pendingDelayMs());
}

// At 60Hz, when the tick returns at slightly after the requested next time,
// make sure a 16ms next delay is posted.
TEST(DelayBasedTimeSource, NextDelaySaneWhenSlightlyAfterRequestedTime) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  // Run the first task, as that activates the timer and picks up a timebase.
  thread.runPendingTask();

  EXPECT_EQ(16, thread.pendingDelayMs());

  timer->setNow(timer->Now() + Interval() +
                base::TimeDelta::FromMicroseconds(1));
  thread.runPendingTask();

  EXPECT_EQ(16, thread.pendingDelayMs());
}

// At 60Hz, when the tick returns at exactly 2*interval after the requested next
// time, make sure a 16ms next delay is posted.
TEST(DelayBasedTimeSource, NextDelaySaneWhenExactlyTwiceAfterRequestedTime) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  // Run the first task, as that activates the timer and picks up a timebase.
  thread.runPendingTask();

  EXPECT_EQ(16, thread.pendingDelayMs());

  timer->setNow(timer->Now() + 2 * Interval());
  thread.runPendingTask();

  EXPECT_EQ(16, thread.pendingDelayMs());
}

// At 60Hz, when the tick returns at 2*interval and a bit after the requested
// next time, make sure a 16ms next delay is posted.
TEST(DelayBasedTimeSource, NextDelaySaneWhenSlightlyAfterTwiceRequestedTime) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  // Run the first task, as that activates the timer and picks up a timebase.
  thread.runPendingTask();

  EXPECT_EQ(16, thread.pendingDelayMs());

  timer->setNow(timer->Now() + 2 * Interval() +
                base::TimeDelta::FromMicroseconds(1));
  thread.runPendingTask();

  EXPECT_EQ(16, thread.pendingDelayMs());
}

// At 60Hz, when the tick returns halfway to the next frame time, make sure
// a correct next delay value is posted.
TEST(DelayBasedTimeSource, NextDelaySaneWhenHalfAfterRequestedTime) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  // Run the first task, as that activates the timer and picks up a timebase.
  thread.runPendingTask();

  EXPECT_EQ(16, thread.pendingDelayMs());

  timer->setNow(timer->Now() + Interval() +
                base::TimeDelta::FromMilliseconds(8));
  thread.runPendingTask();

  EXPECT_EQ(8, thread.pendingDelayMs());
}

// If the timebase and interval are updated with a jittery source, we want to
// make sure we do not double tick.
TEST(DelayBasedTimeSource, SaneHandlingOfJitteryTimebase) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  // Run the first task, as that activates the timer and picks up a timebase.
  thread.runPendingTask();

  EXPECT_EQ(16, thread.pendingDelayMs());

  // Jitter timebase ~1ms late
  timer->setNow(timer->Now() + Interval());
  timer->SetTimebaseAndInterval(
      timer->Now() + base::TimeDelta::FromMilliseconds(1), Interval());
  thread.runPendingTask();

  // Without double tick prevention, pendingDelayMs would be 1.
  EXPECT_EQ(17, thread.pendingDelayMs());

  // Jitter timebase ~1ms early
  timer->setNow(timer->Now() + Interval());
  timer->SetTimebaseAndInterval(
      timer->Now() - base::TimeDelta::FromMilliseconds(1), Interval());
  thread.runPendingTask();

  EXPECT_EQ(15, thread.pendingDelayMs());
}

TEST(DelayBasedTimeSource, HandlesSignificantTimebaseChangesImmediately) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  // Run the first task, as that activates the timer and picks up a timebase.
  thread.runPendingTask();

  EXPECT_EQ(16, thread.pendingDelayMs());

  // Tick, then shift timebase by +7ms.
  timer->setNow(timer->Now() + Interval());
  thread.runPendingTask();

  EXPECT_EQ(16, thread.pendingDelayMs());

  client.reset();
  thread.runPendingTaskOnOverwrite(true);
  base::TimeDelta jitter = base::TimeDelta::FromMilliseconds(7) +
                           base::TimeDelta::FromMicroseconds(1);
  timer->SetTimebaseAndInterval(timer->Now() + jitter, Interval());
  thread.runPendingTaskOnOverwrite(false);

  EXPECT_FALSE(client.tickCalled());  // Make sure pending tasks were canceled.
  EXPECT_EQ(7, thread.pendingDelayMs());

  // Tick, then shift timebase by -7ms.
  timer->setNow(timer->Now() + jitter);
  thread.runPendingTask();

  EXPECT_EQ(16, thread.pendingDelayMs());

  client.reset();
  thread.runPendingTaskOnOverwrite(true);
  timer->SetTimebaseAndInterval(base::TimeTicks() + Interval(), Interval());
  thread.runPendingTaskOnOverwrite(false);

  EXPECT_FALSE(client.tickCalled());  // Make sure pending tasks were canceled.
  EXPECT_EQ(16 - 7, thread.pendingDelayMs());
}

TEST(DelayBasedTimeSource, HanldlesSignificantIntervalChangesImmediately) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);
  // Run the first task, as that activates the timer and picks up a timebase.
  thread.runPendingTask();

  EXPECT_EQ(16, thread.pendingDelayMs());

  // Tick, then double the interval.
  timer->setNow(timer->Now() + Interval());
  thread.runPendingTask();

  EXPECT_EQ(16, thread.pendingDelayMs());

  client.reset();
  thread.runPendingTaskOnOverwrite(true);
  timer->SetTimebaseAndInterval(base::TimeTicks() + Interval(), Interval() * 2);
  thread.runPendingTaskOnOverwrite(false);

  EXPECT_FALSE(client.tickCalled());  // Make sure pending tasks were canceled.
  EXPECT_EQ(33, thread.pendingDelayMs());

  // Tick, then halve the interval.
  timer->setNow(timer->Now() + Interval() * 2);
  thread.runPendingTask();

  EXPECT_EQ(33, thread.pendingDelayMs());

  client.reset();
  thread.runPendingTaskOnOverwrite(true);
  timer->SetTimebaseAndInterval(base::TimeTicks() + Interval() * 3, Interval());
  thread.runPendingTaskOnOverwrite(false);

  EXPECT_FALSE(client.tickCalled());  // Make sure pending tasks were canceled.
  EXPECT_EQ(16, thread.pendingDelayMs());
}

TEST(DelayBasedTimeSourceTest, AchievesTargetRateWithNoNoise) {
  int num_iterations = 10;

  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);

  double total_frame_time = 0.0;
  for (int i = 0; i < num_iterations; ++i) {
    long long delay_ms = thread.pendingDelayMs();

    // accumulate the "delay"
    total_frame_time += delay_ms / 1000.0;

    // Run the callback exactly when asked
    timer->setNow(timer->Now() + base::TimeDelta::FromMilliseconds(delay_ms));
    thread.runPendingTask();
  }
  double average_interval =
      total_frame_time / static_cast<double>(num_iterations);
  EXPECT_NEAR(1.0 / 60.0, average_interval, 0.1);
}

TEST(DelayBasedTimeSource, TestDeactivateWhilePending) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::create(Interval(), &thread);
  timer->SetClient(&client);
  timer->SetActive(true);  // Should post a task.
  timer->SetActive(false);
  timer = NULL;
  thread.runPendingTask();  // Should run the posted task without crashing.
}

TEST(DelayBasedTimeSource, TestDeactivateAndReactivateBeforeNextTickTime) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::create(Interval(), &thread);
  timer->SetClient(&client);

  // Should run the activate task, and pick up a new timebase.
  timer->SetActive(true);
  thread.runPendingTask();

  // Stop the timer
  timer->SetActive(false);

  // Task will be pending anyway, run it
  thread.runPendingTask();

  // Start the timer again, but before the next tick time the timer previously
  // planned on using. That same tick time should still be targeted.
  timer->setNow(timer->Now() + base::TimeDelta::FromMilliseconds(4));
  timer->SetActive(true);
  EXPECT_EQ(12, thread.pendingDelayMs());
}

TEST(DelayBasedTimeSource, TestDeactivateAndReactivateAfterNextTickTime) {
  FakeThread thread;
  FakeTimeSourceClient client;
  scoped_refptr<FakeDelayBasedTimeSource> timer =
      FakeDelayBasedTimeSource::create(Interval(), &thread);
  timer->SetClient(&client);

  // Should run the activate task, and pick up a new timebase.
  timer->SetActive(true);
  thread.runPendingTask();

  // Stop the timer.
  timer->SetActive(false);

  // Task will be pending anyway, run it.
  thread.runPendingTask();

  // Start the timer again, but before the next tick time the timer previously
  // planned on using. That same tick time should still be targeted.
  timer->setNow(timer->Now() + base::TimeDelta::FromMilliseconds(20));
  timer->SetActive(true);
  EXPECT_EQ(13, thread.pendingDelayMs());
}

}  // namespace
}  // namespace cc
