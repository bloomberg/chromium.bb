// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCTimer.h"

#include "CCThread.h"

namespace cc {

class CCTimerTask : public CCThread::Task {
public:
    explicit CCTimerTask(CCTimer* timer)
        : CCThread::Task(0)
        , m_timer(timer)
    {
    }

    ~CCTimerTask()
    {
        if (!m_timer)
            return;

        ASSERT(m_timer->m_task == this);
        m_timer->stop();
    }

    void performTask()
    {
        if (!m_timer)
            return;

        CCTimerClient* client = m_timer->m_client;

        m_timer->stop();
        if (client)
            client->onTimerFired();
    }

private:
    friend class CCTimer;

    CCTimer* m_timer; // null if cancelled
};

CCTimer::CCTimer(CCThread* thread, CCTimerClient* client)
    : m_client(client)
    , m_thread(thread)
    , m_task(0)
{
}

CCTimer::~CCTimer()
{
    stop();
}

void CCTimer::startOneShot(double intervalSeconds)
{
    stop();

    m_task = new CCTimerTask(this);

    // The thread expects delays in milliseconds.
    m_thread->postDelayedTask(adoptPtr(m_task), intervalSeconds * 1000.0);
}

void CCTimer::stop()
{
    if (!m_task)
        return;

    m_task->m_timer = 0;
    m_task = 0;
}

} // namespace cc
