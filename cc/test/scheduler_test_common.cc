// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/scheduler_test_common.h"

#include "base/logging.h"

namespace cc {

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

void FakeThread::runPendingTask()
{
    ASSERT_TRUE(m_pendingTask);
    scoped_ptr<base::Closure> task = m_pendingTask.Pass();
    task->Run();
}

void FakeThread::postTask(base::Closure cb)
{
    postDelayedTask(cb, 0);
}

void FakeThread::postDelayedTask(base::Closure cb, long long delay)
{
    if (m_runPendingTaskOnOverwrite && hasPendingTask())
        runPendingTask();

    ASSERT_FALSE(hasPendingTask());
    m_pendingTask.reset(new base::Closure(cb));
    m_pendingTaskDelay = delay;
}

bool FakeThread::belongsToCurrentThread() const
{
    return true;
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

}  // namespace cc
