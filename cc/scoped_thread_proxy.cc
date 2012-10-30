// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/scoped_thread_proxy.h"

#include "base/bind.h"

namespace cc {

ScopedThreadProxy::ScopedThreadProxy(cc::Thread* targetThread)
    : m_targetThread(targetThread)
    , m_shutdown(false)
{
}

ScopedThreadProxy::~ScopedThreadProxy()
{
}

void ScopedThreadProxy::postTask(const tracked_objects::Location& location, base::Closure cb)
{
    m_targetThread->postTask(base::Bind(&ScopedThreadProxy::runTaskIfNotShutdown, this, cb));
}

void ScopedThreadProxy::shutdown()
{
    DCHECK(m_targetThread->belongsToCurrentThread());
    DCHECK(!m_shutdown);
    m_shutdown = true;
}

void ScopedThreadProxy::runTaskIfNotShutdown(base::Closure cb)
{
    // If our shutdown flag is set, it's possible that m_targetThread has already been destroyed so don't
    // touch it.
    if (m_shutdown) {
        return;
    }
    DCHECK(m_targetThread->belongsToCurrentThread());
    cb.Run();
}

}
