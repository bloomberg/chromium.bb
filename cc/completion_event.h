// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCCompletionEvent_h
#define CCCompletionEvent_h

#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "cc/dcheck.h"

namespace cc {

// Used for making blocking calls from one thread to another. Use only when
// absolutely certain that doing-so will not lead to a deadlock.
//
// It is safe to destroy this object as soon as wait() returns.
class CCCompletionEvent {
public:
    CCCompletionEvent()
        : m_event(false /* manual_reset */, false /* initially_signaled */)
    {
#if CC_DCHECK_ENABLED()
        m_waited = false;
        m_signaled = false;
#endif
    }

    ~CCCompletionEvent()
    {
        DCHECK(m_waited);
        DCHECK(m_signaled);
    }

    void wait()
    {
        DCHECK(!m_waited);
#if CC_DCHECK_ENABLED()
        m_waited = true;
#endif
        base::ThreadRestrictions::ScopedAllowWait allow_wait;
        m_event.Wait();
    }

    void signal()
    {
        DCHECK(!m_signaled);
#if CC_DCHECK_ENABLED()
        m_signaled = true;
#endif
        m_event.Signal();
    }

private:
    base::WaitableEvent m_event;
#if CC_DCHECK_ENABLED()
    // Used to assert that wait() and signal() are each called exactly once.
    bool m_waited;
    bool m_signaled;
#endif
};

}

#endif
