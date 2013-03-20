// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/scheduler_test_common.h"

#include "base/logging.h"

namespace cc {

void FakeTimeSourceClient::OnTimerTick()
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

void FakeThread::PostTask(base::Closure cb)
{
    PostDelayedTask(cb, base::TimeDelta());
}

void FakeThread::PostDelayedTask(base::Closure cb, base::TimeDelta delay)
{
    if (m_runPendingTaskOnOverwrite && hasPendingTask())
        runPendingTask();

    ASSERT_FALSE(hasPendingTask());
    m_pendingTask.reset(new base::Closure(cb));
    m_pendingTaskDelay = delay.InMilliseconds();
}

bool FakeThread::BelongsToCurrentThread() const
{
    return true;
}

void FakeTimeSource::SetClient(TimeSourceClient* client)
{
    m_client = client;
}

void FakeTimeSource::SetActive(bool b)
{
    m_active = b;
}

bool FakeTimeSource::Active() const
{
    return m_active;
}

base::TimeTicks FakeTimeSource::LastTickTime()
{
    return base::TimeTicks();
}

base::TimeTicks FakeTimeSource::NextTickTime()
{
    return base::TimeTicks();
}

base::TimeTicks FakeDelayBasedTimeSource::Now() const
{
    return m_now;
}

}  // namespace cc
