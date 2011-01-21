// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_thread.h"

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/threading/thread_restrictions.h"

// Friendly names for the well-known threads.
static const char* browser_thread_names[BrowserThread::ID_COUNT] = {
  "",  // UI (name assembled in browser_main.cc).
  "Chrome_DBThread",  // DB
  "Chrome_WebKitThread",  // WEBKIT
  "Chrome_FileThread",  // FILE
  "Chrome_ProcessLauncherThread",  // PROCESS_LAUNCHER
  "Chrome_CacheThread",  // CACHE
  "Chrome_IOThread",  // IO
#if defined(USE_X11)
  "Chrome_Background_X11Thread",  // BACKGROUND_X11
#endif
};

// An implementation of MessageLoopProxy to be used in conjunction
// with BrowserThread.
class BrowserThreadMessageLoopProxy : public base::MessageLoopProxy {
 public:
  explicit BrowserThreadMessageLoopProxy(BrowserThread::ID identifier)
      : id_(identifier) {
  }

  // MessageLoopProxy implementation.
  virtual bool PostTask(const tracked_objects::Location& from_here,
                        Task* task) {
    return BrowserThread::PostTask(id_, from_here, task);
  }

  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               Task* task, int64 delay_ms) {
    return BrowserThread::PostDelayedTask(id_, from_here, task, delay_ms);
  }

  virtual bool PostNonNestableTask(const tracked_objects::Location& from_here,
                                   Task* task) {
    return BrowserThread::PostNonNestableTask(id_, from_here, task);
  }

  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      Task* task,
      int64 delay_ms) {
    return BrowserThread::PostNonNestableDelayedTask(id_, from_here, task,
                                                    delay_ms);
  }
  virtual bool BelongsToCurrentThread() {
    return BrowserThread::CurrentlyOn(id_);
  }

 private:
  BrowserThread::ID id_;
  DISALLOW_COPY_AND_ASSIGN(BrowserThreadMessageLoopProxy);
};


base::Lock BrowserThread::lock_;

BrowserThread* BrowserThread::browser_threads_[ID_COUNT];

BrowserThread::BrowserThread(BrowserThread::ID identifier)
    : Thread(browser_thread_names[identifier]),
      identifier_(identifier) {
  Initialize();
}

BrowserThread::BrowserThread(ID identifier, MessageLoop* message_loop)
    : Thread(message_loop->thread_name().c_str()),
      identifier_(identifier) {
  set_message_loop(message_loop);
  Initialize();
}

void BrowserThread::Initialize() {
  base::AutoLock lock(lock_);
  DCHECK(identifier_ >= 0 && identifier_ < ID_COUNT);
  DCHECK(browser_threads_[identifier_] == NULL);
  browser_threads_[identifier_] = this;
}

BrowserThread::~BrowserThread() {
  // Stop the thread here, instead of the parent's class destructor.  This is so
  // that if there are pending tasks that run, code that checks that it's on the
  // correct BrowserThread succeeds.
  Stop();

  base::AutoLock lock(lock_);
  browser_threads_[identifier_] = NULL;
#ifndef NDEBUG
  // Double check that the threads are ordered correctly in the enumeration.
  for (int i = identifier_ + 1; i < ID_COUNT; ++i) {
    DCHECK(!browser_threads_[i]) <<
        "Threads must be listed in the reverse order that they die";
  }
#endif
}

// static
bool BrowserThread::IsWellKnownThread(ID identifier) {
  base::AutoLock lock(lock_);
  return (identifier >= 0 && identifier < ID_COUNT &&
          browser_threads_[identifier]);
}

// static
bool BrowserThread::CurrentlyOn(ID identifier) {
  // We shouldn't use MessageLoop::current() since it uses LazyInstance which
  // may be deleted by ~AtExitManager when a WorkerPool thread calls this
  // function.
  // http://crbug.com/63678
  base::ThreadRestrictions::ScopedAllowSingleton allow_singleton;
  base::AutoLock lock(lock_);
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  return browser_threads_[identifier] &&
         browser_threads_[identifier]->message_loop() == MessageLoop::current();
}

// static
bool BrowserThread::IsMessageLoopValid(ID identifier) {
  base::AutoLock lock(lock_);
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  return browser_threads_[identifier] &&
         browser_threads_[identifier]->message_loop();
}

// static
bool BrowserThread::PostTask(ID identifier,
                            const tracked_objects::Location& from_here,
                            Task* task) {
  return PostTaskHelper(identifier, from_here, task, 0, true);
}

// static
bool BrowserThread::PostDelayedTask(ID identifier,
                                   const tracked_objects::Location& from_here,
                                   Task* task,
                                   int64 delay_ms) {
  return PostTaskHelper(identifier, from_here, task, delay_ms, true);
}

// static
bool BrowserThread::PostNonNestableTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    Task* task) {
  return PostTaskHelper(identifier, from_here, task, 0, false);
}

// static
bool BrowserThread::PostNonNestableDelayedTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    Task* task,
    int64 delay_ms) {
  return PostTaskHelper(identifier, from_here, task, delay_ms, false);
}

// static
bool BrowserThread::GetCurrentThreadIdentifier(ID* identifier) {
  // We shouldn't use MessageLoop::current() since it uses LazyInstance which
  // may be deleted by ~AtExitManager when a WorkerPool thread calls this
  // function.
  // http://crbug.com/63678
  base::ThreadRestrictions::ScopedAllowSingleton allow_singleton;
  MessageLoop* cur_message_loop = MessageLoop::current();
  for (int i = 0; i < ID_COUNT; ++i) {
    if (browser_threads_[i] &&
        browser_threads_[i]->message_loop() == cur_message_loop) {
      *identifier = browser_threads_[i]->identifier_;
      return true;
    }
  }

  return false;
}

// static
scoped_refptr<base::MessageLoopProxy>
BrowserThread::GetMessageLoopProxyForThread(
    ID identifier) {
  scoped_refptr<base::MessageLoopProxy> proxy(
      new BrowserThreadMessageLoopProxy(identifier));
  return proxy;
}

// static
bool BrowserThread::PostTaskHelper(
    ID identifier,
    const tracked_objects::Location& from_here,
    Task* task,
    int64 delay_ms,
    bool nestable) {
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  // Optimization: to avoid unnecessary locks, we listed the ID enumeration in
  // order of lifetime.  So no need to lock if we know that the other thread
  // outlives this one.
  // Note: since the array is so small, ok to loop instead of creating a map,
  // which would require a lock because std::map isn't thread safe, defeating
  // the whole purpose of this optimization.
  ID current_thread;
  bool guaranteed_to_outlive_target_thread =
      GetCurrentThreadIdentifier(&current_thread) &&
      current_thread >= identifier;

  if (!guaranteed_to_outlive_target_thread)
    lock_.Acquire();

  MessageLoop* message_loop = browser_threads_[identifier] ?
      browser_threads_[identifier]->message_loop() : NULL;
  if (message_loop) {
    if (nestable) {
      message_loop->PostDelayedTask(from_here, task, delay_ms);
    } else {
      message_loop->PostNonNestableDelayedTask(from_here, task, delay_ms);
    }
  } else {
    delete task;
  }

  if (!guaranteed_to_outlive_target_thread)
    lock_.Release();

  return !!message_loop;
}
