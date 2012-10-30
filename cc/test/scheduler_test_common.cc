// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/test/scheduler_test_common.h"

#include "base/logging.h"

namespace WebKitTests {

void FakeTimeSourceClient::onTimerTick()
{
    m_tickCalled = true;
}

FakeThread::FakeThread()
{
    reset();
}

FakeThread::~FakeThread()
{
}

void FakeThread::postTask(PassOwnPtr<Task>)
{
    NOTREACHED();
}

void FakeThread::postDelayedTask(PassOwnPtr<Task> task, long long delay)
{
    if (m_runPendingTaskOnOverwrite && hasPendingTask())
        runPendingTask();

    EXPECT_TRUE(!hasPendingTask());
    m_pendingTask = task;
    m_pendingTaskDelay = delay;
}

base::PlatformThreadId FakeThread::threadID() const
{
    return 0;
}

void FakeTimeSource::setClient(cc::TimeSourceClient* client)
{
    m_client = client;
}

void FakeTimeSource::setActive(bool b)
{
    m_active = b;
}

bool FakeTimeSource::active() const
{
    return m_active;
}

base::TimeTicks FakeTimeSource::lastTickTime()
{
    return base::TimeTicks();
}

base::TimeTicks FakeTimeSource::nextTickTime()
{
    return base::TimeTicks();
}

base::TimeTicks FakeDelayBasedTimeSource::now() const
{
    return m_now;
}

}
