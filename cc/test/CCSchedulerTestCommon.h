// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCSchedulerTestCommon_h
#define CCSchedulerTestCommon_h

#include "CCDelayBasedTimeSource.h"
#include "CCFrameRateController.h"
#include "CCThread.h"
#include "base/threading/platform_thread.h"
#include <gtest/gtest.h>
#include <wtf/OwnPtr.h>

namespace WebKitTests {

class FakeCCTimeSourceClient : public cc::CCTimeSourceClient {
public:
    FakeCCTimeSourceClient() { reset(); }
    void reset() { m_tickCalled = false; }
    bool tickCalled() const { return m_tickCalled; }

    virtual void onTimerTick() OVERRIDE;

protected:
    bool m_tickCalled;
};

class FakeCCThread : public cc::CCThread {
public:
    FakeCCThread();
    virtual ~FakeCCThread();

    void reset()
    {
        m_pendingTaskDelay = 0;
        m_pendingTask.clear();
        m_runPendingTaskOnOverwrite = false;
    }

    void runPendingTaskOnOverwrite(bool enable)
    {
        m_runPendingTaskOnOverwrite = enable;
    }

    bool hasPendingTask() const { return m_pendingTask; }
    void runPendingTask()
    {
        ASSERT(m_pendingTask);
        OwnPtr<Task> task = m_pendingTask.release();
        task->performTask();
    }

    long long pendingDelayMs() const
    {
        EXPECT_TRUE(hasPendingTask());
        return m_pendingTaskDelay;
    }

    virtual void postTask(PassOwnPtr<Task>) OVERRIDE;
    virtual void postDelayedTask(PassOwnPtr<Task> task, long long delay) OVERRIDE;
    virtual base::PlatformThreadId threadID() const OVERRIDE;

protected:
    OwnPtr<Task> m_pendingTask;
    long long m_pendingTaskDelay;
    bool m_runPendingTaskOnOverwrite;
};

class FakeCCTimeSource : public cc::CCTimeSource {
public:
    FakeCCTimeSource()
        : m_active(false)
        , m_client(0)
    {
        turnOffVerifier();
    }

    virtual ~FakeCCTimeSource() { }

    virtual void setClient(cc::CCTimeSourceClient* client) OVERRIDE;
    virtual void setActive(bool b) OVERRIDE;
    virtual bool active() const OVERRIDE;
    virtual void setTimebaseAndInterval(base::TimeTicks timebase, base::TimeDelta interval) OVERRIDE { }
    virtual base::TimeTicks lastTickTime() OVERRIDE;
    virtual base::TimeTicks nextTickTimeIfActivated() OVERRIDE;

    void tick()
    {
        ASSERT(m_active);
        if (m_client)
            m_client->onTimerTick();
    }

    void setNextTickTime(base::TimeTicks nextTickTime) { m_nextTickTime = nextTickTime; }

protected:
    bool m_active;
    base::TimeTicks m_nextTickTime;
    cc::CCTimeSourceClient* m_client;
};

class FakeCCDelayBasedTimeSource : public cc::CCDelayBasedTimeSource {
public:
    static PassRefPtr<FakeCCDelayBasedTimeSource> create(base::TimeDelta interval, cc::CCThread* thread)
    {
        return adoptRef(new FakeCCDelayBasedTimeSource(interval, thread));
    }

    void setNow(base::TimeTicks time) { m_now = time; }
    virtual base::TimeTicks now() const OVERRIDE;

protected:
    FakeCCDelayBasedTimeSource(base::TimeDelta interval, cc::CCThread* thread)
        : CCDelayBasedTimeSource(interval, thread)
    {
    }

    base::TimeTicks m_now;
};

class FakeCCFrameRateController : public cc::CCFrameRateController {
public:
    FakeCCFrameRateController(PassRefPtr<cc::CCTimeSource> timer) : cc::CCFrameRateController(timer) { }

    int numFramesPending() const { return m_numFramesPending; }
};

}

#endif // CCSchedulerTestCommon_h
