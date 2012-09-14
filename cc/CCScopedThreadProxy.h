// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCScopedThreadProxy_h
#define CCScopedThreadProxy_h

#include "CCThreadTask.h"
#include "base/threading/platform_thread.h"
#include <wtf/ThreadSafeRefCounted.h>

namespace cc {

// This class is a proxy used to post tasks to an target thread from any other thread. The proxy may be shut down at
// any point from the target thread after which no more tasks posted to the proxy will run. In other words, all
// tasks posted via a proxy are scoped to the lifecycle of the proxy. Use this when posting tasks to an object that
// might die with tasks in flight.
//
// The proxy must be created and shut down from the target thread, tasks may be posted from any thread.
//
// Implementation note: Unlike ScopedRunnableMethodFactory in Chromium, pending tasks are not cancelled by actually
// destroying the proxy. Instead each pending task holds a reference to the proxy to avoid maintaining an explicit
// list of outstanding tasks.
class CCScopedThreadProxy : public ThreadSafeRefCounted<CCScopedThreadProxy> {
public:
    static PassRefPtr<CCScopedThreadProxy> create(CCThread* targetThread)
    {
        ASSERT(base::PlatformThread::CurrentId() == targetThread->threadID());
        return adoptRef(new CCScopedThreadProxy(targetThread));
    }

    // Can be called from any thread. Posts a task to the target thread that runs unless
    // shutdown() is called before it runs.
    void postTask(PassOwnPtr<CCThread::Task> task)
    {
        ref();
        m_targetThread->postTask(createCCThreadTask(this, &CCScopedThreadProxy::runTaskIfNotShutdown, task));
    }

    void shutdown()
    {
        ASSERT(base::PlatformThread::CurrentId() == m_targetThread->threadID());
        ASSERT(!m_shutdown);
        m_shutdown = true;
    }

private:
    explicit CCScopedThreadProxy(CCThread* targetThread)
        : m_targetThread(targetThread)
        , m_shutdown(false) { }

    void runTaskIfNotShutdown(PassOwnPtr<CCThread::Task> popTask)
    {
        OwnPtr<CCThread::Task> task = popTask;
        // If our shutdown flag is set, it's possible that m_targetThread has already been destroyed so don't
        // touch it.
        if (m_shutdown) {
            deref();
            return;
        }
        ASSERT(base::PlatformThread::CurrentId() == m_targetThread->threadID());
        task->performTask();
        deref();
    }

    CCThread* m_targetThread;
    bool m_shutdown; // Only accessed on the target thread
};

}

#endif
