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

    virtual void onTimerTick() OVERRIDE { m_tickCalled = true; }

protected:
    bool m_tickCalled;
};

class FakeCCThread : public cc::CCThread {
public:
    FakeCCThread() { reset(); }
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

    virtual void postTask(PassOwnPtr<Task>) { ASSERT_NOT_REACHED(); }
    virtual void postDelayedTask(PassOwnPtr<Task> task, long long delay)
    {
        if (m_runPendingTaskOnOverwrite && hasPendingTask())
            runPendingTask();

        EXPECT_TRUE(!hasPendingTask());
        m_pendingTask = task;
        m_pendingTaskDelay = delay;
    }
    virtual base::PlatformThreadId threadID() const { return 0; }

protected:
    OwnPtr<Task> m_pendingTask;
    long long m_pendingTaskDelay;
    bool m_runPendingTaskOnOverwrite;
};

class FakeCCTimeSource : public cc::CCTimeSource {
public:
    FakeCCTimeSource()
        : m_active(false)
        , m_nextTickTime(0)
        , m_client(0)
    {
        turnOffVerifier();
    }

    virtual ~FakeCCTimeSource() { }

    virtual void setClient(cc::CCTimeSourceClient* client) OVERRIDE { m_client = client; }
    virtual void setActive(bool b) OVERRIDE { m_active = b; }
    virtual bool active() const OVERRIDE { return m_active; }
    virtual void setTimebaseAndInterval(double timebase, double interval) OVERRIDE { }
    virtual double lastTickTime() OVERRIDE { return 0; }
    virtual double nextTickTimeIfActivated() OVERRIDE { return 0; }

    void tick()
    {
        ASSERT(m_active);
        if (m_client)
            m_client->onTimerTick();
    }

    void setNextTickTime(double nextTickTime) { m_nextTickTime = nextTickTime; }

protected:
    bool m_active;
    double m_nextTickTime;
    cc::CCTimeSourceClient* m_client;
};

class FakeCCDelayBasedTimeSource : public cc::CCDelayBasedTimeSource {
public:
    static PassRefPtr<FakeCCDelayBasedTimeSource> create(double interval, cc::CCThread* thread)
    {
        return adoptRef(new FakeCCDelayBasedTimeSource(interval, thread));
    }

    void setMonotonicTimeNow(double time) { m_monotonicTimeNow = time; }
    virtual double monotonicTimeNow() const OVERRIDE { return m_monotonicTimeNow; }

protected:
    FakeCCDelayBasedTimeSource(double interval, cc::CCThread* thread)
        : CCDelayBasedTimeSource(interval, thread)
        , m_monotonicTimeNow(0) { }

    double m_monotonicTimeNow;
};

class FakeCCFrameRateController : public cc::CCFrameRateController {
public:
    FakeCCFrameRateController(PassRefPtr<cc::CCTimeSource> timer) : cc::CCFrameRateController(timer) { }

    int numFramesPending() const { return m_numFramesPending; }
};

}

#endif // CCSchedulerTestCommon_h
