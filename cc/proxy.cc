// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/proxy.h"

#include "cc/thread.h"

namespace cc {

namespace {
#ifndef NDEBUG
bool implThreadIsOverridden = false;
bool s_isMainThreadBlocked = false;
#endif
Thread* s_mainThread = 0;
Thread* s_implThread = 0;
}

void Proxy::setMainThread(Thread* thread)
{
    s_mainThread = thread;
}

Thread* Proxy::mainThread()
{
    return s_mainThread;
}

bool Proxy::hasImplThread()
{
    return s_implThread;
}

void Proxy::setImplThread(Thread* thread)
{
    s_implThread = thread;
}

Thread* Proxy::implThread()
{
    return s_implThread;
}

Thread* Proxy::currentThread()
{
    if (s_mainThread && s_mainThread->belongsToCurrentThread())
        return s_mainThread;
    if (s_implThread && s_implThread->belongsToCurrentThread())
        return s_implThread;
    return 0;
}

bool Proxy::isMainThread()
{
#ifndef NDEBUG
    DCHECK(s_mainThread);
    if (implThreadIsOverridden)
        return false;
    return s_mainThread->belongsToCurrentThread();
#else
    return true;
#endif
}

bool Proxy::isImplThread()
{
#ifndef NDEBUG
    if (implThreadIsOverridden)
        return true;
    return s_implThread && s_implThread->belongsToCurrentThread();
#else
    return true;
#endif
}

#ifndef NDEBUG
void Proxy::setCurrentThreadIsImplThread(bool isImplThread)
{
    implThreadIsOverridden = isImplThread;
}
#endif

bool Proxy::isMainThreadBlocked()
{
#ifndef NDEBUG
    return s_isMainThreadBlocked;
#else
    return true;
#endif
}

#ifndef NDEBUG
void Proxy::setMainThreadBlocked(bool isMainThreadBlocked)
{
    s_isMainThreadBlocked = isMainThreadBlocked;
}
#endif

Proxy::Proxy()
{
    DCHECK(isMainThread());
}

Proxy::~Proxy()
{
    DCHECK(isMainThread());
}

}
