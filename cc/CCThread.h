// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCThread_h
#define CCThread_h

#include "base/threading/platform_thread.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/Noncopyable.h>

namespace cc {

// CCThread provides basic infrastructure for messaging with the compositor in a
// platform-neutral way.
class CCThread {
public:
    virtual ~CCThread() { }

    class Task {
        WTF_MAKE_NONCOPYABLE(Task);
    public:
        virtual ~Task() { }
        virtual void performTask() = 0;
        void* instance() const { return m_instance; }
    protected:
        Task(void* instance) : m_instance(instance) { }
        void* m_instance;
    };

    // Executes the task on context's thread asynchronously.
    virtual void postTask(PassOwnPtr<Task>) = 0;

    // Executes the task after the specified delay.
    virtual void postDelayedTask(PassOwnPtr<Task>, long long delayMs) = 0;

    virtual base::PlatformThreadId threadID() const = 0;
};

}

#endif
