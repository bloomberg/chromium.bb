// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_COMPLETION_EVENT_H_
#define CC_COMPLETION_EVENT_H_

#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/logging.h"

namespace cc {

// Used for making blocking calls from one thread to another. Use only when
// absolutely certain that doing-so will not lead to a deadlock.
//
// It is safe to destroy this object as soon as wait() returns.
class CompletionEvent {
public:
    CompletionEvent()
        : m_event(false /* manual_reset */, false /* initially_signaled */)
    {
#ifndef NDEBUG
        m_waited = false;
        m_signaled = false;
#endif
    }

    ~CompletionEvent()
    {
#ifndef NDEBUG
        DCHECK(m_waited);
        DCHECK(m_signaled);
#endif
    }

    void wait()
    {
#ifndef NDEBUG
        DCHECK(!m_waited);
        m_waited = true;
#endif
        base::ThreadRestrictions::ScopedAllowWait allow_wait;
        m_event.Wait();
    }

    void signal()
    {
#ifndef NDEBUG
        DCHECK(!m_signaled);
        m_signaled = true;
#endif
        m_event.Signal();
    }

private:
    base::WaitableEvent m_event;
#ifndef NDEBUG
    // Used to assert that wait() and signal() are each called exactly once.
    bool m_waited;
    bool m_signaled;
#endif
};

}

#endif  // CC_COMPLETION_EVENT_H_
