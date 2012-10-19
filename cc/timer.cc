// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCTimer.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "CCThread.h"

namespace cc {

class TimerTask : public Thread::Task {
public:
    explicit TimerTask(Timer* timer)
        : Thread::Task(0)
        , m_timer(timer)
    {
    }

    virtual ~TimerTask()
    {
        if (!m_timer)
            return;

        DCHECK(m_timer->m_task == this);
        m_timer->stop();
    }

    virtual void performTask() OVERRIDE
    {
        if (!m_timer)
            return;

        TimerClient* client = m_timer->m_client;

        m_timer->stop();
        if (client)
            client->onTimerFired();
    }

private:
    friend class Timer;

    Timer* m_timer; // null if cancelled
};

Timer::Timer(Thread* thread, TimerClient* client)
    : m_client(client)
    , m_thread(thread)
    , m_task(0)
{
}

Timer::~Timer()
{
    stop();
}

void Timer::startOneShot(double intervalSeconds)
{
    stop();

    m_task = new TimerTask(this);

    // The thread expects delays in milliseconds.
    m_thread->postDelayedTask(adoptPtr(m_task), intervalSeconds * 1000.0);
}

void Timer::stop()
{
    if (!m_task)
        return;

    m_task->m_timer = 0;
    m_task = 0;
}

} // namespace cc
