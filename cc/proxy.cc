// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCProxy.h"

#include "CCThreadTask.h"

using namespace WTF;

namespace cc {

namespace {
#ifndef NDEBUG
bool implThreadIsOverridden = false;
bool s_isMainThreadBlocked = false;
base::PlatformThreadId threadIDOverridenToBeImplThread;
#endif
CCThread* s_mainThread = 0;
CCThread* s_implThread = 0;
}

void CCProxy::setMainThread(CCThread* thread)
{
    s_mainThread = thread;
}

CCThread* CCProxy::mainThread()
{
    return s_mainThread;
}

bool CCProxy::hasImplThread()
{
    return s_implThread;
}

void CCProxy::setImplThread(CCThread* thread)
{
    s_implThread = thread;
}

CCThread* CCProxy::implThread()
{
    return s_implThread;
}

CCThread* CCProxy::currentThread()
{
    base::PlatformThreadId currentThreadIdentifier = base::PlatformThread::CurrentId();
    if (s_mainThread && s_mainThread->threadID() == currentThreadIdentifier)
        return s_mainThread;
    if (s_implThread && s_implThread->threadID() == currentThreadIdentifier)
        return s_implThread;
    return 0;
}

#ifndef NDEBUG
bool CCProxy::isMainThread()
{
    ASSERT(s_mainThread);
    if (implThreadIsOverridden && base::PlatformThread::CurrentId() == threadIDOverridenToBeImplThread)
        return false;
    return base::PlatformThread::CurrentId() == s_mainThread->threadID();
}

bool CCProxy::isImplThread()
{
    base::PlatformThreadId implThreadID = s_implThread ? s_implThread->threadID() : 0;
    if (implThreadIsOverridden && base::PlatformThread::CurrentId() == threadIDOverridenToBeImplThread)
        return true;
    return base::PlatformThread::CurrentId() == implThreadID;
}

void CCProxy::setCurrentThreadIsImplThread(bool isImplThread)
{
    implThreadIsOverridden = isImplThread;
    if (isImplThread)
        threadIDOverridenToBeImplThread = base::PlatformThread::CurrentId();
}

bool CCProxy::isMainThreadBlocked()
{
    return s_isMainThreadBlocked;
}

void CCProxy::setMainThreadBlocked(bool isMainThreadBlocked)
{
    s_isMainThreadBlocked = isMainThreadBlocked;
}
#endif

CCProxy::CCProxy()
{
    ASSERT(isMainThread());
}

CCProxy::~CCProxy()
{
    ASSERT(isMainThread());
}

}
