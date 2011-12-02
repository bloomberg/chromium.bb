// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_thread_impl.h"

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/threading/thread_restrictions.h"

namespace content {

namespace {

// Friendly names for the well-known threads.
static const char* g_browser_thread_names[BrowserThread::ID_COUNT] = {
  "",  // UI (name assembled in browser_main.cc).
  "Chrome_DBThread",  // DB
  "Chrome_WebKitThread",  // WEBKIT
  "Chrome_FileThread",  // FILE
  "Chrome_ProcessLauncherThread",  // PROCESS_LAUNCHER
  "Chrome_CacheThread",  // CACHE
  "Chrome_IOThread",  // IO
#if defined(OS_CHROMEOS)
  "Chrome_WebSocketproxyThread",  // WEB_SOCKET_PROXY
#endif
};

// This lock protects |g_browser_threads|.  Do not read or modify that
// array without holding this lock.  Do not block while holding this
// lock.
base::LazyInstance<base::Lock,
                   base::LeakyLazyInstanceTraits<base::Lock> >
    g_lock = LAZY_INSTANCE_INITIALIZER;

// This array is protected by |g_lock|.  The threads are not owned by this
// array. Typically, the threads are owned on the UI thread by
// content::BrowserMainLoop.  BrowserThreadImpl objects remove
// themselves from this array upon destruction.
static BrowserThreadImpl* g_browser_threads[BrowserThread::ID_COUNT];

// Only atomic operations are used on this array.  The delegates are
// not owned by this array, rather by whoever calls
// BrowserThread::SetDelegate.
static BrowserThreadDelegate* g_browser_thread_delegates[
    BrowserThread::ID_COUNT];

}  // namespace

BrowserThreadImpl::BrowserThreadImpl(ID identifier)
    : Thread(g_browser_thread_names[identifier]),
      identifier_(identifier) {
  Initialize();
}

BrowserThreadImpl::BrowserThreadImpl(ID identifier,
                                     MessageLoop* message_loop)
    : Thread(message_loop->thread_name().c_str()),
      identifier_(identifier) {
  set_message_loop(message_loop);
  Initialize();
}

void BrowserThreadImpl::Init() {
  using base::subtle::AtomicWord;
  AtomicWord* storage =
      reinterpret_cast<AtomicWord*>(&g_browser_thread_delegates[identifier_]);
  AtomicWord stored_pointer = base::subtle::NoBarrier_Load(storage);
  BrowserThreadDelegate* delegate =
      reinterpret_cast<BrowserThreadDelegate*>(stored_pointer);
  if (delegate)
    delegate->Init();
}

void BrowserThreadImpl::CleanUp() {
  using base::subtle::AtomicWord;
  AtomicWord* storage =
      reinterpret_cast<AtomicWord*>(&g_browser_thread_delegates[identifier_]);
  AtomicWord stored_pointer = base::subtle::NoBarrier_Load(storage);
  BrowserThreadDelegate* delegate =
      reinterpret_cast<BrowserThreadDelegate*>(stored_pointer);

  if (delegate)
    delegate->CleanUp();
}

void BrowserThreadImpl::Initialize() {
  base::AutoLock lock(g_lock.Get());
  DCHECK(identifier_ >= 0 && identifier_ < ID_COUNT);
  DCHECK(g_browser_threads[identifier_] == NULL);
  g_browser_threads[identifier_] = this;
}

BrowserThreadImpl::~BrowserThreadImpl() {
  // All Thread subclasses must call Stop() in the destructor. This is
  // doubly important here as various bits of code check they are on
  // the right BrowserThread.
  Stop();

  base::AutoLock lock(g_lock.Get());
  g_browser_threads[identifier_] = NULL;
#ifndef NDEBUG
  // Double check that the threads are ordered correctly in the enumeration.
  for (int i = identifier_ + 1; i < ID_COUNT; ++i) {
    DCHECK(!g_browser_threads[i]) <<
        "Threads must be listed in the reverse order that they die";
  }
#endif
}

// static
bool BrowserThreadImpl::PostTaskHelper(
    BrowserThread::ID identifier,
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
  BrowserThread::ID current_thread;
  bool guaranteed_to_outlive_target_thread =
      GetCurrentThreadIdentifier(&current_thread) &&
      current_thread <= identifier;

  if (!guaranteed_to_outlive_target_thread)
    g_lock.Get().Acquire();

  MessageLoop* message_loop = g_browser_threads[identifier] ?
      g_browser_threads[identifier]->message_loop() : NULL;
  if (message_loop) {
    if (nestable) {
      message_loop->PostDelayedTask(from_here, task, delay_ms);
    } else {
      message_loop->PostNonNestableDelayedTask(from_here, task, delay_ms);
    }
  }

  if (!guaranteed_to_outlive_target_thread)
    g_lock.Get().Release();

  if (!message_loop)
    delete task;

  return !!message_loop;
}

// static
bool BrowserThreadImpl::PostTaskHelper(
    BrowserThread::ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    int64 delay_ms,
    bool nestable) {
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  // Optimization: to avoid unnecessary locks, we listed the ID enumeration in
  // order of lifetime.  So no need to lock if we know that the other thread
  // outlives this one.
  // Note: since the array is so small, ok to loop instead of creating a map,
  // which would require a lock because std::map isn't thread safe, defeating
  // the whole purpose of this optimization.
  BrowserThread::ID current_thread;
  bool guaranteed_to_outlive_target_thread =
      GetCurrentThreadIdentifier(&current_thread) &&
      current_thread <= identifier;

  if (!guaranteed_to_outlive_target_thread)
    g_lock.Get().Acquire();

  MessageLoop* message_loop = g_browser_threads[identifier] ?
      g_browser_threads[identifier]->message_loop() : NULL;
  if (message_loop) {
    if (nestable) {
      message_loop->PostDelayedTask(from_here, task, delay_ms);
    } else {
      message_loop->PostNonNestableDelayedTask(from_here, task, delay_ms);
    }
  }

  if (!guaranteed_to_outlive_target_thread)
    g_lock.Get().Release();

  return !!message_loop;
}

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

  virtual bool PostTask(const tracked_objects::Location& from_here,
                        const base::Closure& task) {
    return BrowserThread::PostTask(id_, from_here, task);
  }

  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task, int64 delay_ms) {
    return BrowserThread::PostDelayedTask(id_, from_here, task, delay_ms);
  }

  virtual bool PostNonNestableTask(const tracked_objects::Location& from_here,
                                   const base::Closure& task) {
    return BrowserThread::PostNonNestableTask(id_, from_here, task);
  }

  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
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

// static
bool BrowserThread::IsWellKnownThread(ID identifier) {
  base::AutoLock lock(g_lock.Get());
  return (identifier >= 0 && identifier < ID_COUNT &&
          g_browser_threads[identifier]);
}

// static
bool BrowserThread::CurrentlyOn(ID identifier) {
  // We shouldn't use MessageLoop::current() since it uses LazyInstance which
  // may be deleted by ~AtExitManager when a WorkerPool thread calls this
  // function.
  // http://crbug.com/63678
  base::ThreadRestrictions::ScopedAllowSingleton allow_singleton;
  base::AutoLock lock(g_lock.Get());
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  return g_browser_threads[identifier] &&
         g_browser_threads[identifier]->message_loop() ==
             MessageLoop::current();
}

// static
bool BrowserThread::IsMessageLoopValid(ID identifier) {
  base::AutoLock lock(g_lock.Get());
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  return g_browser_threads[identifier] &&
         g_browser_threads[identifier]->message_loop();
}

// static
bool BrowserThread::PostTask(ID identifier,
                             const tracked_objects::Location& from_here,
                             const base::Closure& task) {
  return BrowserThreadImpl::PostTaskHelper(
      identifier, from_here, task, 0, true);
}

// static
bool BrowserThread::PostDelayedTask(ID identifier,
                                    const tracked_objects::Location& from_here,
                                    const base::Closure& task,
                                    int64 delay_ms) {
  return BrowserThreadImpl::PostTaskHelper(
      identifier, from_here, task, delay_ms, true);
}

// static
bool BrowserThread::PostNonNestableTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return BrowserThreadImpl::PostTaskHelper(
      identifier, from_here, task, 0, false);
}

// static
bool BrowserThread::PostNonNestableDelayedTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    int64 delay_ms) {
  return BrowserThreadImpl::PostTaskHelper(
      identifier, from_here, task, delay_ms, false);
}

// static
bool BrowserThread::PostTask(ID identifier,
                             const tracked_objects::Location& from_here,
                             Task* task) {
  return BrowserThreadImpl::PostTaskHelper(
      identifier, from_here, task, 0, true);
}

// static
bool BrowserThread::PostDelayedTask(ID identifier,
                                    const tracked_objects::Location& from_here,
                                    Task* task,
                                    int64 delay_ms) {
  return BrowserThreadImpl::PostTaskHelper(
      identifier, from_here, task, delay_ms, true);
}

// static
bool BrowserThread::PostNonNestableTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    Task* task) {
  return BrowserThreadImpl::PostTaskHelper(
      identifier, from_here, task, 0, false);
}

// static
bool BrowserThread::PostNonNestableDelayedTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    Task* task,
    int64 delay_ms) {
  return BrowserThreadImpl::PostTaskHelper(
      identifier, from_here, task, delay_ms, false);
}

// static
bool BrowserThread::PostTaskAndReply(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    const base::Closure& reply) {
  return GetMessageLoopProxyForThread(identifier)->PostTaskAndReply(from_here,
                                                                    task,
                                                                    reply);
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
    if (g_browser_threads[i] &&
        g_browser_threads[i]->message_loop() == cur_message_loop) {
      *identifier = g_browser_threads[i]->identifier_;
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

base::Thread* BrowserThread::UnsafeGetBrowserThread(ID identifier) {
  base::AutoLock lock(g_lock.Get());
  base::Thread* thread = g_browser_threads[identifier];
  DCHECK(thread);
  return thread;
}

MessageLoop* BrowserThread::UnsafeGetMessageLoop(ID identifier) {
  return UnsafeGetBrowserThread(identifier)->message_loop();
}

void BrowserThread::SetDelegate(ID identifier,
                                BrowserThreadDelegate* delegate) {
  using base::subtle::AtomicWord;
  AtomicWord* storage = reinterpret_cast<AtomicWord*>(
      &g_browser_thread_delegates[identifier]);
  AtomicWord old_pointer = base::subtle::NoBarrier_AtomicExchange(
      storage, reinterpret_cast<AtomicWord>(delegate));

  // This catches registration when previously registered.
  DCHECK(!delegate || !old_pointer);
}

}  // namespace content
