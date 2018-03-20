// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_thread_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"

namespace content {

namespace {

// An implementation of SingleThreadTaskRunner to be used in conjunction
// with BrowserThread.
// TODO(gab): Consider replacing this with |g_globals->task_runners| -- only
// works if none are requested before starting the threads.
class BrowserThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit BrowserThreadTaskRunner(BrowserThread::ID identifier)
      : id_(identifier) {}

  // SingleThreadTaskRunner implementation.
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return BrowserThread::PostDelayedTask(id_, from_here, std::move(task),
                                          delay);
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return BrowserThread::PostNonNestableDelayedTask(id_, from_here,
                                                     std::move(task), delay);
  }

  bool RunsTasksInCurrentSequence() const override {
    return BrowserThread::CurrentlyOn(id_);
  }

 protected:
  ~BrowserThreadTaskRunner() override {}

 private:
  BrowserThread::ID id_;
  DISALLOW_COPY_AND_ASSIGN(BrowserThreadTaskRunner);
};

// A separate helper is used just for the task runners, in order to avoid
// needing to initialize the globals to create a task runner.
struct BrowserThreadTaskRunners {
  BrowserThreadTaskRunners() {
    for (int i = 0; i < BrowserThread::ID_COUNT; ++i) {
      proxies[i] =
          new BrowserThreadTaskRunner(static_cast<BrowserThread::ID>(i));
    }
  }

  scoped_refptr<base::SingleThreadTaskRunner> proxies[BrowserThread::ID_COUNT];
};

base::LazyInstance<BrowserThreadTaskRunners>::Leaky g_task_runners =
    LAZY_INSTANCE_INITIALIZER;

// State of a given BrowserThread::ID in chronological order throughout the
// browser process' lifetime.
enum BrowserThreadState {
  // BrowserThread::ID isn't associated with anything yet.
  UNINITIALIZED = 0,
  // BrowserThread::ID is associated to a TaskRunner and is accepting tasks.
  RUNNING,
  // BrowserThread::ID no longer accepts tasks.
  SHUTDOWN
};

struct BrowserThreadGlobals {
  // This lock protects |task_runners| and |states|. Do not read or modify those
  // arrays without holding this lock. Do not block while holding this lock.
  base::Lock lock;

  // This array is filled either as the underlying threads start and invoke
  // Init() or in BrowserThreadImpl() when a MessageLoop* is provided at
  // construction. It is not emptied during shutdown in order to support
  // RunsTasksInCurrentSequence() until the very end.
  scoped_refptr<base::SingleThreadTaskRunner>
      task_runners[BrowserThread::ID_COUNT];

  // Holds the state of each BrowserThread::ID.
  BrowserThreadState states[BrowserThread::ID_COUNT] = {};
};

base::LazyInstance<BrowserThreadGlobals>::Leaky
    g_globals = LAZY_INSTANCE_INITIALIZER;

bool PostTaskHelper(BrowserThread::ID identifier,
                    const base::Location& from_here,
                    base::OnceClosure task,
                    base::TimeDelta delay,
                    bool nestable) {
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, BrowserThread::ID_COUNT);
  // Optimization: to avoid unnecessary locks, we listed the ID enumeration in
  // order of lifetime.  So no need to lock if we know that the target thread
  // outlives current thread as that implies the current thread only ever sees
  // the target thread in its RUNNING state.
  // Note: since the array is so small, ok to loop instead of creating a map,
  // which would require a lock because std::map isn't thread safe, defeating
  // the whole purpose of this optimization.
  BrowserThread::ID current_thread = BrowserThread::ID_COUNT;
  bool target_thread_outlives_current =
      BrowserThreadImpl::GetCurrentThreadIdentifier(&current_thread) &&
      current_thread >= identifier;

  BrowserThreadGlobals& globals = g_globals.Get();
  if (!target_thread_outlives_current)
    globals.lock.Acquire();

  const bool accepting_tasks =
      globals.states[identifier] == BrowserThreadState::RUNNING;
  if (accepting_tasks) {
    base::SingleThreadTaskRunner* task_runner =
        globals.task_runners[identifier].get();
    DCHECK(task_runner);
    if (nestable) {
      task_runner->PostDelayedTask(from_here, std::move(task), delay);
    } else {
      task_runner->PostNonNestableDelayedTask(from_here, std::move(task),
                                              delay);
    }
  }

  if (!target_thread_outlives_current)
    globals.lock.Release();

  return accepting_tasks;
}

}  // namespace

BrowserThreadImpl::BrowserThreadImpl(
    ID identifier,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : identifier_(identifier) {
  DCHECK(task_runner);

  BrowserThreadGlobals& globals = g_globals.Get();

  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier_, 0);
  DCHECK_LT(identifier_, ID_COUNT);
  DCHECK_EQ(globals.states[identifier_], BrowserThreadState::UNINITIALIZED);
  globals.states[identifier_] = BrowserThreadState::RUNNING;
  globals.task_runners[identifier_] = std::move(task_runner);
}

BrowserThreadImpl::~BrowserThreadImpl() {
  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);

  DCHECK_EQ(globals.states[identifier_], BrowserThreadState::RUNNING);
  globals.states[identifier_] = BrowserThreadState::SHUTDOWN;
}

// static
void BrowserThreadImpl::ResetGlobalsForTesting(BrowserThread::ID identifier) {
  BrowserThreadGlobals& globals = g_globals.Get();

  base::AutoLock lock(globals.lock);
  DCHECK_EQ(globals.states[identifier], BrowserThreadState::SHUTDOWN);
  globals.states[identifier] = BrowserThreadState::UNINITIALIZED;
  globals.task_runners[identifier] = nullptr;
}

// static
const char* BrowserThreadImpl::GetThreadName(BrowserThread::ID thread) {
  static const char* const kBrowserThreadNames[BrowserThread::ID_COUNT] = {
      "",                 // UI (name assembled in browser_main_loop.cc).
      "Chrome_IOThread",  // IO
  };

  if (BrowserThread::UI < thread && thread < BrowserThread::ID_COUNT)
    return kBrowserThreadNames[thread];
  if (thread == BrowserThread::UI)
    return "Chrome_UIThread";
  return "Unknown Thread";
}

// static
void BrowserThread::PostAfterStartupTask(
    const base::Location& from_here,
    const scoped_refptr<base::TaskRunner>& task_runner,
    base::OnceClosure task) {
  GetContentClient()->browser()->PostAfterStartupTask(from_here, task_runner,
                                                      std::move(task));
}

// static
bool BrowserThread::IsThreadInitialized(ID identifier) {
  if (!g_globals.IsCreated())
    return false;

  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  return globals.states[identifier] == BrowserThreadState::RUNNING;
}

// static
bool BrowserThread::CurrentlyOn(ID identifier) {
  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  return globals.task_runners[identifier] &&
         globals.task_runners[identifier]->RunsTasksInCurrentSequence();
}

// static
std::string BrowserThread::GetDCheckCurrentlyOnErrorMessage(ID expected) {
  std::string actual_name = base::PlatformThread::GetName();
  if (actual_name.empty())
    actual_name = "Unknown Thread";

  std::string result = "Must be called on ";
  result += BrowserThreadImpl::GetThreadName(expected);
  result += "; actually called on ";
  result += actual_name;
  result += ".";
  return result;
}

// static
bool BrowserThread::IsMessageLoopValid(ID identifier) {
  if (!g_globals.IsCreated())
    return false;

  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  return globals.states[identifier] == BrowserThreadState::RUNNING;
}

// static
bool BrowserThread::PostTask(ID identifier,
                             const base::Location& from_here,
                             base::OnceClosure task) {
  return PostTaskHelper(identifier, from_here, std::move(task),
                        base::TimeDelta(), true);
}

// static
bool BrowserThread::PostDelayedTask(ID identifier,
                                    const base::Location& from_here,
                                    base::OnceClosure task,
                                    base::TimeDelta delay) {
  return PostTaskHelper(identifier, from_here, std::move(task), delay, true);
}

// static
bool BrowserThread::PostNonNestableTask(ID identifier,
                                        const base::Location& from_here,
                                        base::OnceClosure task) {
  return PostTaskHelper(identifier, from_here, std::move(task),
                        base::TimeDelta(), false);
}

// static
bool BrowserThread::PostNonNestableDelayedTask(ID identifier,
                                               const base::Location& from_here,
                                               base::OnceClosure task,
                                               base::TimeDelta delay) {
  return PostTaskHelper(identifier, from_here, std::move(task), delay, false);
}

// static
bool BrowserThread::PostTaskAndReply(ID identifier,
                                     const base::Location& from_here,
                                     base::OnceClosure task,
                                     base::OnceClosure reply) {
  return GetTaskRunnerForThread(identifier)
      ->PostTaskAndReply(from_here, std::move(task), std::move(reply));
}

// static
bool BrowserThread::GetCurrentThreadIdentifier(ID* identifier) {
  if (!g_globals.IsCreated())
    return false;

  BrowserThreadGlobals& globals = g_globals.Get();
  // Profiler to track potential contention on |globals.lock|. This only does
  // real work on canary and local dev builds, so the cost of having this here
  // should be minimal.
  base::AutoLock lock(globals.lock);
  for (int i = 0; i < ID_COUNT; ++i) {
    if (globals.task_runners[i] &&
        globals.task_runners[i]->RunsTasksInCurrentSequence()) {
      *identifier = static_cast<ID>(i);
      return true;
    }
  }

  return false;
}

// static
scoped_refptr<base::SingleThreadTaskRunner>
BrowserThread::GetTaskRunnerForThread(ID identifier) {
  return g_task_runners.Get().proxies[identifier];
}

}  // namespace content
