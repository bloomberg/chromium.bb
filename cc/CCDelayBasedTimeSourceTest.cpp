// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCDelayBasedTimeSource.h"

#include "CCSchedulerTestCommon.h"
#include "CCThread.h"
#include <gtest/gtest.h>
#include <wtf/RefPtr.h>

using namespace cc;
using namespace WTF;
using namespace WebKitTests;

namespace {

base::TimeDelta interval()
{
    return base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond / 60);
}

TEST(CCDelayBasedTimeSourceTest, TaskPostedAndTickCalled)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval(), &thread);
    timer->setClient(&client);

    timer->setActive(true);
    EXPECT_TRUE(timer->active());
    EXPECT_TRUE(thread.hasPendingTask());

    timer->setNow(timer->now() + base::TimeDelta::FromMilliseconds(16));
    thread.runPendingTask();
    EXPECT_TRUE(timer->active());
    EXPECT_TRUE(client.tickCalled());
}

TEST(CCDelayBasedTimeSource, TickNotCalledWithTaskPosted)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval(), &thread);
    timer->setClient(&client);
    timer->setActive(true);
    EXPECT_TRUE(thread.hasPendingTask());
    timer->setActive(false);
    thread.runPendingTask();
    EXPECT_FALSE(client.tickCalled());
}

TEST(CCDelayBasedTimeSource, StartTwiceEnqueuesOneTask)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval(), &thread);
    timer->setClient(&client);
    timer->setActive(true);
    EXPECT_TRUE(thread.hasPendingTask());
    thread.reset();
    timer->setActive(true);
    EXPECT_FALSE(thread.hasPendingTask());
}

TEST(CCDelayBasedTimeSource, StartWhenRunningDoesntTick)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval(), &thread);
    timer->setClient(&client);
    timer->setActive(true);
    thread.runPendingTask();
    thread.reset();
    timer->setActive(true);
    EXPECT_FALSE(thread.hasPendingTask());
}

// At 60Hz, when the tick returns at exactly the requested next time, make sure
// a 16ms next delay is posted.
TEST(CCDelayBasedTimeSource, NextDelaySaneWhenExactlyOnRequestedTime)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval(), &thread);
    timer->setClient(&client);
    timer->setActive(true);
    // Run the first task, as that activates the timer and picks up a timebase.
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelayMs());

    timer->setNow(timer->now() + interval());
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelayMs());
}

// At 60Hz, when the tick returns at slightly after the requested next time, make sure
// a 16ms next delay is posted.
TEST(CCDelayBasedTimeSource, NextDelaySaneWhenSlightlyAfterRequestedTime)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval(), &thread);
    timer->setClient(&client);
    timer->setActive(true);
    // Run the first task, as that activates the timer and picks up a timebase.
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelayMs());

    timer->setNow(timer->now() + interval() + base::TimeDelta::FromMicroseconds(1));
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelayMs());
}

// At 60Hz, when the tick returns at exactly 2*interval after the requested next time, make sure
// a 16ms next delay is posted.
TEST(CCDelayBasedTimeSource, NextDelaySaneWhenExactlyTwiceAfterRequestedTime)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval(), &thread);
    timer->setClient(&client);
    timer->setActive(true);
    // Run the first task, as that activates the timer and picks up a timebase.
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelayMs());

    timer->setNow(timer->now() + 2 * interval());
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelayMs());
}

// At 60Hz, when the tick returns at 2*interval and a bit after the requested next time, make sure
// a 16ms next delay is posted.
TEST(CCDelayBasedTimeSource, NextDelaySaneWhenSlightlyAfterTwiceRequestedTime)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval(), &thread);
    timer->setClient(&client);
    timer->setActive(true);
    // Run the first task, as that activates the timer and picks up a timebase.
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelayMs());

    timer->setNow(timer->now() + 2 * interval() + base::TimeDelta::FromMicroseconds(1));
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelayMs());
}

// At 60Hz, when the tick returns halfway to the next frame time, make sure
// a correct next delay value is posted.
TEST(CCDelayBasedTimeSource, NextDelaySaneWhenHalfAfterRequestedTime)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval(), &thread);
    timer->setClient(&client);
    timer->setActive(true);
    // Run the first task, as that activates the timer and picks up a timebase.
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelayMs());

    timer->setNow(timer->now() + interval() + base::TimeDelta::FromMilliseconds(8));
    thread.runPendingTask();

    EXPECT_EQ(8, thread.pendingDelayMs());
}

// If the timebase and interval are updated with a jittery source, we want to
// make sure we do not double tick.
TEST(CCDelayBasedTimeSource, SaneHandlingOfJitteryTimebase)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval(), &thread);
    timer->setClient(&client);
    timer->setActive(true);
    // Run the first task, as that activates the timer and picks up a timebase.
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelayMs());

    // Jitter timebase ~1ms late
    timer->setNow(timer->now() + interval());
    timer->setTimebaseAndInterval(timer->now() + base::TimeDelta::FromMilliseconds(1), interval());
    thread.runPendingTask();

    // Without double tick prevention, pendingDelayMs would be 1.
    EXPECT_EQ(17, thread.pendingDelayMs());

    // Jitter timebase ~1ms early
    timer->setNow(timer->now() + interval());
    timer->setTimebaseAndInterval(timer->now() - base::TimeDelta::FromMilliseconds(1), interval());
    thread.runPendingTask();

    EXPECT_EQ(15, thread.pendingDelayMs());
}

TEST(CCDelayBasedTimeSource, HandlesSignificantTimebaseChangesImmediately)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval(), &thread);
    timer->setClient(&client);
    timer->setActive(true);
    // Run the first task, as that activates the timer and picks up a timebase.
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelayMs());

    // Tick, then shift timebase by +7ms.
    timer->setNow(timer->now() + interval());
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelayMs());

    client.reset();
    thread.runPendingTaskOnOverwrite(true);
    base::TimeDelta jitter = base::TimeDelta::FromMilliseconds(7) + base::TimeDelta::FromMicroseconds(1);
    timer->setTimebaseAndInterval(timer->now() + jitter, interval());
    thread.runPendingTaskOnOverwrite(false);

    EXPECT_FALSE(client.tickCalled()); // Make sure pending tasks were canceled.
    EXPECT_EQ(7, thread.pendingDelayMs());

    // Tick, then shift timebase by -7ms.
    timer->setNow(timer->now() + jitter);
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelayMs());

    client.reset();
    thread.runPendingTaskOnOverwrite(true);
    timer->setTimebaseAndInterval(base::TimeTicks() + interval(), interval());
    thread.runPendingTaskOnOverwrite(false);

    EXPECT_FALSE(client.tickCalled()); // Make sure pending tasks were canceled.
    EXPECT_EQ(16-7, thread.pendingDelayMs());
}

TEST(CCDelayBasedTimeSource, HanldlesSignificantIntervalChangesImmediately)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval(), &thread);
    timer->setClient(&client);
    timer->setActive(true);
    // Run the first task, as that activates the timer and picks up a timebase.
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelayMs());

    // Tick, then double the interval.
    timer->setNow(timer->now() + interval());
    thread.runPendingTask();

    EXPECT_EQ(16, thread.pendingDelayMs());

    client.reset();
    thread.runPendingTaskOnOverwrite(true);
    timer->setTimebaseAndInterval(base::TimeTicks() + interval(), interval() * 2);
    thread.runPendingTaskOnOverwrite(false);

    EXPECT_FALSE(client.tickCalled()); // Make sure pending tasks were canceled.
    EXPECT_EQ(33, thread.pendingDelayMs());

    // Tick, then halve the interval.
    timer->setNow(timer->now() + interval() * 2);
    thread.runPendingTask();

    EXPECT_EQ(33, thread.pendingDelayMs());

    client.reset();
    thread.runPendingTaskOnOverwrite(true);
    timer->setTimebaseAndInterval(base::TimeTicks() + interval() * 3, interval());
    thread.runPendingTaskOnOverwrite(false);

    EXPECT_FALSE(client.tickCalled()); // Make sure pending tasks were canceled.
    EXPECT_EQ(16, thread.pendingDelayMs());
}

TEST(CCDelayBasedTimeSourceTest, AchievesTargetRateWithNoNoise)
{
    int numIterations = 10;

    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval(), &thread);
    timer->setClient(&client);
    timer->setActive(true);

    double totalFrameTime = 0;
    for (int i = 0; i < numIterations; ++i) {
        long long delayMs = thread.pendingDelayMs();

        // accumulate the "delay"
        totalFrameTime += delayMs / 1000.0;

        // Run the callback exactly when asked
        timer->setNow(timer->now() + base::TimeDelta::FromMilliseconds(delayMs));
        thread.runPendingTask();
    }
    double averageInterval = totalFrameTime / static_cast<double>(numIterations);
    EXPECT_NEAR(1.0 / 60.0, averageInterval, 0.1);
}

TEST(CCDelayBasedTimeSource, TestDeactivateWhilePending)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval(), &thread);
    timer->setClient(&client);
    timer->setActive(true); // Should post a task.
    timer->setActive(false);
    timer.clear();
    thread.runPendingTask(); // Should run the posted task without crashing.
}

TEST(CCDelayBasedTimeSource, TestDeactivateAndReactivateBeforeNextTickTime)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval(), &thread);
    timer->setClient(&client);

    // Should run the activate task, and pick up a new timebase.
    timer->setActive(true);
    thread.runPendingTask();

    // Stop the timer
    timer->setActive(false);

    // Task will be pending anyway, run it
    thread.runPendingTask();

    // Start the timer again, but before the next tick time the timer previously
    // planned on using. That same tick time should still be targeted.
    timer->setNow(timer->now() + base::TimeDelta::FromMilliseconds(4));
    timer->setActive(true);
    EXPECT_EQ(12, thread.pendingDelayMs());
}

TEST(CCDelayBasedTimeSource, TestDeactivateAndReactivateAfterNextTickTime)
{
    FakeCCThread thread;
    FakeCCTimeSourceClient client;
    RefPtr<FakeCCDelayBasedTimeSource> timer = FakeCCDelayBasedTimeSource::create(interval(), &thread);
    timer->setClient(&client);

    // Should run the activate task, and pick up a new timebase.
    timer->setActive(true);
    thread.runPendingTask();

    // Stop the timer
    timer->setActive(false);

    // Task will be pending anyway, run it
    thread.runPendingTask();

    // Start the timer again, but before the next tick time the timer previously
    // planned on using. That same tick time should still be targeted.
    timer->setNow(timer->now() + base::TimeDelta::FromMilliseconds(20));
    timer->setActive(true);
    EXPECT_EQ(13, thread.pendingDelayMs());
}

}
