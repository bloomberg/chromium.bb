// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCScopedThreadProxy_h
#define CCScopedThreadProxy_h

#include "base/memory/ref_counted.h"
#include "base/callback.h"
#include "cc/thread.h"
#include "base/location.h"
#include "base/logging.h"

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
class ScopedThreadProxy : public base::RefCountedThreadSafe<ScopedThreadProxy> {
public:
    static scoped_refptr<ScopedThreadProxy> create(cc::Thread* targetThread)
    {
        DCHECK(targetThread->belongsToCurrentThread());
        return make_scoped_refptr(new ScopedThreadProxy(targetThread));
    }

    // Can be called from any thread. Posts a task to the target thread that runs unless
    // shutdown() is called before it runs.
    void postTask(const tracked_objects::Location& location, base::Closure cb);

    void shutdown();

private:
    explicit ScopedThreadProxy(cc::Thread* targetThread);
    friend class base::RefCountedThreadSafe<ScopedThreadProxy>;
    ~ScopedThreadProxy();

    void runTaskIfNotShutdown(base::Closure cb);

    cc::Thread* m_targetThread;
    bool m_shutdown; // Only accessed on the target thread
};

}  // namespace cc

#endif
