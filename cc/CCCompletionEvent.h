// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCCompletionEvent_h
#define CCCompletionEvent_h

#include <wtf/ThreadingPrimitives.h>

namespace WebCore {

// Used for making blocking calls from one thread to another. Use only when
// absolutely certain that doing-so will not lead to a deadlock.
//
// It is safe to destroy this object as soon as wait() returns.
class CCCompletionEvent {
public:
    CCCompletionEvent()
    {
#ifndef NDEBUG
        m_waited = false;
        m_signaled = false;
#endif
        m_mutex.lock();
    }

    ~CCCompletionEvent()
    {
        m_mutex.unlock();
        ASSERT(m_waited);
        ASSERT(m_signaled);
    }

    void wait()
    {
        ASSERT(!m_waited);
#ifndef NDEBUG
        m_waited = true;
#endif
        m_condition.wait(m_mutex);
    }

    void signal()
    {
        MutexLocker lock(m_mutex);
        ASSERT(!m_signaled);
#ifndef NDEBUG
        m_signaled = true;
#endif
        m_condition.signal();
    }

private:
    Mutex m_mutex;
    ThreadCondition m_condition;
#ifndef NDEBUG
    // Used to assert that wait() and signal() are each called exactly once.
    bool m_waited;
    bool m_signaled;
#endif
};

}

#endif
