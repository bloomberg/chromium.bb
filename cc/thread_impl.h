// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_THREAD_IMPL_H_
#define CC_THREAD_IMPL_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "cc/cc_export.h"
#include "cc/thread.h"

namespace cc {

// Implements cc::Thread in terms of base::MessageLoopProxy.
class CC_EXPORT ThreadImpl : public Thread {
public:
    // Creates a ThreadImpl wrapping the current thread.
    static scoped_ptr<cc::Thread> createForCurrentThread();

    // Creates a Thread wrapping a non-current thread.
    static scoped_ptr<cc::Thread> createForDifferentThread(scoped_refptr<base::MessageLoopProxy> thread);

    virtual ~ThreadImpl();

    // cc::Thread implementation
    virtual void postTask(base::Closure cb) OVERRIDE;
    virtual void postDelayedTask(base::Closure cb, long long delayMs) OVERRIDE;
    virtual bool belongsToCurrentThread() const OVERRIDE;

private:
    explicit ThreadImpl(scoped_refptr<base::MessageLoopProxy> thread);

    scoped_refptr<base::MessageLoopProxy> m_thread;
};

} // namespace cc

#endif  // CC_THREAD_IMPL_H_
