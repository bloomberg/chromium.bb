// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/thread_impl.h"

#include "base/message_loop.h"

namespace cc {

scoped_ptr<Thread> ThreadImpl::createForCurrentThread()
{
    return scoped_ptr<Thread>(new ThreadImpl(base::MessageLoopProxy::current())).Pass();
}

scoped_ptr<Thread> ThreadImpl::createForDifferentThread(scoped_refptr<base::MessageLoopProxy> thread)
{
    return scoped_ptr<Thread>(new ThreadImpl(thread)).Pass();
}

ThreadImpl::~ThreadImpl()
{
}

void ThreadImpl::postTask(base::Closure cb)
{
    m_thread->PostTask(FROM_HERE, cb);
}

void ThreadImpl::postDelayedTask(base::Closure cb, long long delayMs)
{
    m_thread->PostDelayedTask(FROM_HERE, cb, base::TimeDelta::FromMilliseconds(delayMs));
}

bool ThreadImpl::belongsToCurrentThread() const
{
  return m_thread->BelongsToCurrentThread();
}

ThreadImpl::ThreadImpl(scoped_refptr<base::MessageLoopProxy> thread)
    : m_thread(thread)
{
}

}  // namespace cc
