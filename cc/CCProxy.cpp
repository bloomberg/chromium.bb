// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCProxy.h"

#include "CCThreadTask.h"
#include <wtf/MainThread.h>

using namespace WTF;

namespace WebCore {

namespace {
#ifndef NDEBUG
bool implThreadIsOverridden = false;
bool s_isMainThreadBlocked = false;
ThreadIdentifier threadIDOverridenToBeImplThread;
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
    ThreadIdentifier currentThreadIdentifier = WTF::currentThread();
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
    if (implThreadIsOverridden && WTF::currentThread() == threadIDOverridenToBeImplThread)
        return false;
    return WTF::currentThread() == s_mainThread->threadID();
}

bool CCProxy::isImplThread()
{
    WTF::ThreadIdentifier implThreadID = s_implThread ? s_implThread->threadID() : 0;
    if (implThreadIsOverridden && WTF::currentThread() == threadIDOverridenToBeImplThread)
        return true;
    return WTF::currentThread() == implThreadID;
}

void CCProxy::setCurrentThreadIsImplThread(bool isImplThread)
{
    implThreadIsOverridden = isImplThread;
    if (isImplThread)
        threadIDOverridenToBeImplThread = WTF::currentThread();
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
