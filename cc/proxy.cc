// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/proxy.h"

#include "cc/thread.h"
#include "cc/thread_impl.h"

namespace cc {

Thread* Proxy::mainThread() const
{
    return m_mainThread.get();
}

bool Proxy::hasImplThread() const
{
    return m_implThread;
}

Thread* Proxy::implThread() const
{
    return m_implThread.get();
}

Thread* Proxy::currentThread() const
{
    if (mainThread() && mainThread()->belongsToCurrentThread())
        return mainThread();
    if (implThread() && implThread()->belongsToCurrentThread())
        return implThread();
    return 0;
}

bool Proxy::isMainThread() const
{
#ifndef NDEBUG
    DCHECK(mainThread());
    if (m_implThreadIsOverridden)
        return false;
    return mainThread()->belongsToCurrentThread();
#else
    return true;
#endif
}

bool Proxy::isImplThread() const
{
#ifndef NDEBUG
    if (m_implThreadIsOverridden)
        return true;
    return implThread() && implThread()->belongsToCurrentThread();
#else
    return true;
#endif
}

#ifndef NDEBUG
void Proxy::setCurrentThreadIsImplThread(bool isImplThread)
{
    m_implThreadIsOverridden = isImplThread;
}
#endif

bool Proxy::isMainThreadBlocked() const
{
#ifndef NDEBUG
    return m_isMainThreadBlocked;
#else
    return true;
#endif
}

#ifndef NDEBUG
void Proxy::setMainThreadBlocked(bool isMainThreadBlocked)
{
    m_isMainThreadBlocked = isMainThreadBlocked;
}
#endif

Proxy::Proxy(scoped_ptr<Thread> implThread)
    : m_mainThread(ThreadImpl::createForCurrentThread())
    , m_implThread(implThread.Pass())
#ifndef NDEBUG
    , m_implThreadIsOverridden(false)
    , m_isMainThreadBlocked(false)
#endif
{
}

Proxy::~Proxy()
{
    DCHECK(isMainThread());
}

}  // namespace cc
