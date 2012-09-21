// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCSchedulerTestCommon.h"

namespace WebKitTests {

void FakeCCTimeSourceClient::onTimerTick()
{
    m_tickCalled = true;
}

FakeCCThread::FakeCCThread()
{
    reset();
}

FakeCCThread::~FakeCCThread()
{
}

void FakeCCThread::postTask(PassOwnPtr<Task>)
{
    ASSERT_NOT_REACHED();
}

void FakeCCThread::postDelayedTask(PassOwnPtr<Task> task, long long delay)
{
    if (m_runPendingTaskOnOverwrite && hasPendingTask())
        runPendingTask();

    EXPECT_TRUE(!hasPendingTask());
    m_pendingTask = task;
    m_pendingTaskDelay = delay;
}

base::PlatformThreadId FakeCCThread::threadID() const
{
    return 0;
}

void FakeCCTimeSource::setClient(cc::CCTimeSourceClient* client)
{
    m_client = client;
}

void FakeCCTimeSource::setActive(bool b)
{
    m_active = b;
}

bool FakeCCTimeSource::active() const
{
    return m_active;
}

base::TimeTicks FakeCCTimeSource::lastTickTime()
{
    return base::TimeTicks();
}

base::TimeTicks FakeCCTimeSource::nextTickTimeIfActivated()
{
    return base::TimeTicks();
}

base::TimeTicks FakeCCDelayBasedTimeSource::now() const
{
    return m_now;
}

}
