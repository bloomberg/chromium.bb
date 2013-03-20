// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SCHEDULER_TEST_COMMON_H_
#define CC_TEST_SCHEDULER_TEST_COMMON_H_

#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cc/base/thread.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/scheduler/frame_rate_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class FakeTimeSourceClient : public cc::TimeSourceClient {
public:
    FakeTimeSourceClient() { reset(); }
    void reset() { m_tickCalled = false; }
    bool tickCalled() const { return m_tickCalled; }

    virtual void onTimerTick() OVERRIDE;

protected:
    bool m_tickCalled;
};

class FakeThread : public cc::Thread {
public:
    FakeThread();
    virtual ~FakeThread();

    void reset()
    {
        m_pendingTaskDelay = 0;
        m_pendingTask.reset();
        m_runPendingTaskOnOverwrite = false;
    }

    void runPendingTaskOnOverwrite(bool enable)
    {
        m_runPendingTaskOnOverwrite = enable;
    }

    bool hasPendingTask() const { return m_pendingTask; }
    void runPendingTask();

    long long pendingDelayMs() const
    {
        EXPECT_TRUE(hasPendingTask());
        return m_pendingTaskDelay;
    }

    virtual void PostTask(base::Closure cb) OVERRIDE;
    virtual void PostDelayedTask(base::Closure cb, base::TimeDelta delay)
        OVERRIDE;
    virtual bool BelongsToCurrentThread() const OVERRIDE;

protected:
    scoped_ptr<base::Closure> m_pendingTask;
    long long m_pendingTaskDelay;
    bool m_runPendingTaskOnOverwrite;
};

class FakeTimeSource : public cc::TimeSource {
public:
    FakeTimeSource()
        : m_active(false)
        , m_client(0)
    {
    }

    virtual void setClient(cc::TimeSourceClient* client) OVERRIDE;
    virtual void setActive(bool b) OVERRIDE;
    virtual bool active() const OVERRIDE;
    virtual void setTimebaseAndInterval(base::TimeTicks timebase, base::TimeDelta interval) OVERRIDE { }
    virtual base::TimeTicks lastTickTime() OVERRIDE;
    virtual base::TimeTicks nextTickTime() OVERRIDE;

    void tick()
    {
        ASSERT_TRUE(m_active);
        if (m_client)
            m_client->onTimerTick();
    }

    void setNextTickTime(base::TimeTicks nextTickTime) { m_nextTickTime = nextTickTime; }

protected:
    virtual ~FakeTimeSource() { }

    bool m_active;
    base::TimeTicks m_nextTickTime;
    cc::TimeSourceClient* m_client;
};

class FakeDelayBasedTimeSource : public cc::DelayBasedTimeSource {
public:
    static scoped_refptr<FakeDelayBasedTimeSource> create(base::TimeDelta interval, cc::Thread* thread)
    {
        return make_scoped_refptr(new FakeDelayBasedTimeSource(interval, thread));
    }

    void setNow(base::TimeTicks time) { m_now = time; }
    virtual base::TimeTicks now() const OVERRIDE;

protected:
    FakeDelayBasedTimeSource(base::TimeDelta interval, cc::Thread* thread)
        : DelayBasedTimeSource(interval, thread)
    {
    }
    virtual ~FakeDelayBasedTimeSource() { }

    base::TimeTicks m_now;
};

class FakeFrameRateController : public cc::FrameRateController {
public:
    FakeFrameRateController(scoped_refptr<cc::TimeSource> timer) : cc::FrameRateController(timer) { }

    int numFramesPending() const { return num_frames_pending_; }
};

}  // namespace cc

#endif  // CC_TEST_SCHEDULER_TEST_COMMON_H_
