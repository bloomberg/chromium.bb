// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_thread_impl.h"

#include <string>
#include <utility>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/task/task_executor.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
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
  // BrowserThread::ID no longer accepts tasks (it's still associated to a
  // TaskRunner but that TaskRunner doesn't have to accept tasks).
  SHUTDOWN
};

struct BrowserThreadGlobals {
  BrowserThreadGlobals() {
    // A few unit tests which do not use a TestBrowserThreadBundle still invoke
    // code that reaches into CurrentlyOn()/IsThreadInitialized(). This can
    // result in instantiating BrowserThreadGlobals off the main thread.
    // |main_thread_checker_| being bound incorrectly would then result in a
    // flake in the next test that instantiates a TestBrowserThreadBundle in the
    // same process. Detaching here postpones binding |main_thread_checker_| to
    // the first invocation of BrowserThreadImpl::BrowserThreadImpl() and works
    // around this issue.
    DETACH_FROM_THREAD(main_thread_checker_);
  }

  // BrowserThreadGlobals must be initialized on main thread before it's used by
  // any other threads.
  THREAD_CHECKER(main_thread_checker_);

  // |task_runners[id]| is safe to access on |main_thread_checker_| as
  // well as on any thread once it's read-only after initialization
  // (i.e. while |states[id] >= RUNNING|).
  scoped_refptr<base::SingleThreadTaskRunner>
      task_runners[BrowserThread::ID_COUNT];

  // Tracks the runtime state of BrowserThreadImpls. Atomic because a few
  // methods below read this value outside |main_thread_checker_| to
  // confirm it's >= RUNNING and doing so requires an atomic read as it could be
  // in the middle of transitioning to SHUTDOWN (which the check is fine with
  // but reading a non-atomic value as it's written to by another thread can
  // result in undefined behaviour on some platforms).
  // Only NoBarrier atomic operations should be used on |states| as it shouldn't
  // be used to establish happens-after relationships but rather checking the
  // runtime state of various threads (once again: it's only atomic to support
  // reading while transitioning from RUNNING=>SHUTDOWN).
  base::subtle::Atomic32 states[BrowserThread::ID_COUNT] = {};
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

  BrowserThreadGlobals& globals = g_globals.Get();

  // Tasks should always be posted while the BrowserThread is in a RUNNING or
  // SHUTDOWN state (will return false if SHUTDOWN).
  //
  // Posting tasks before BrowserThreads are initialized is incorrect as it
  // would silently no-op. If you need to support posting early, gate it on
  // BrowserThread::IsThreadInitialized(). If you hit this check in unittests,
  // you most likely posted a task outside the scope of a
  // TestBrowserThreadBundle (which also completely resets the state after
  // shutdown in ~TestBrowserThreadBundle(), ref. ResetGlobalsForTesting(),
  // making sure TestBrowserThreadBundle is the first member of your test
  // fixture and thus outlives everything is usually the right solution).
  DCHECK_GE(base::subtle::NoBarrier_Load(&globals.states[identifier]),
            BrowserThreadState::RUNNING);
  DCHECK(globals.task_runners[identifier]);

  if (nestable) {
    return globals.task_runners[identifier]->PostDelayedTask(
        from_here, std::move(task), delay);
  } else {
    return globals.task_runners[identifier]->PostNonNestableDelayedTask(
        from_here, std::move(task), delay);
  }
}

class BrowserThreadTaskExecutor : public base::TaskExecutor {
 public:
  BrowserThreadTaskExecutor() {}
  ~BrowserThreadTaskExecutor() override {}

  // base::TaskExecutor implementation.
  bool PostDelayedTaskWithTraits(const base::Location& from_here,
                                 const base::TaskTraits& traits,
                                 base::OnceClosure task,
                                 base::TimeDelta delay) override {
    return PostTaskHelper(
        GetBrowserThreadIdentifier(traits), from_here, std::move(task), delay,
        traits.GetExtension<BrowserTaskTraitsExtension>().nestable());
  }

  scoped_refptr<base::TaskRunner> CreateTaskRunnerWithTraits(
      const base::TaskTraits& traits) override {
    return GetTaskRunnerForThread(GetBrowserThreadIdentifier(traits));
  }

  scoped_refptr<base::SequencedTaskRunner> CreateSequencedTaskRunnerWithTraits(
      const base::TaskTraits& traits) override {
    return GetTaskRunnerForThread(GetBrowserThreadIdentifier(traits));
  }

  scoped_refptr<base::SingleThreadTaskRunner>
  CreateSingleThreadTaskRunnerWithTraits(
      const base::TaskTraits& traits,
      base::SingleThreadTaskRunnerThreadMode thread_mode) override {
    // It's not possible to request DEDICATED access to a BrowserThread.
    DCHECK_EQ(thread_mode, base::SingleThreadTaskRunnerThreadMode::SHARED);
    return GetTaskRunnerForThread(GetBrowserThreadIdentifier(traits));
  }

#if defined(OS_WIN)
  scoped_refptr<base::SingleThreadTaskRunner> CreateCOMSTATaskRunnerWithTraits(
      const base::TaskTraits& traits,
      base::SingleThreadTaskRunnerThreadMode thread_mode) override {
    // Only the UI thread supports COM.
    DCHECK_EQ(GetBrowserThreadIdentifier(traits), BrowserThread::UI);
    return CreateSingleThreadTaskRunnerWithTraits(traits, thread_mode);
  }
#endif  // defined(OS_WIN)

 private:
  BrowserThread::ID GetBrowserThreadIdentifier(const base::TaskTraits& traits) {
    DCHECK_EQ(traits.extension_id(), BrowserTaskTraitsExtension::kExtensionId);
    BrowserThread::ID id =
        traits.GetExtension<BrowserTaskTraitsExtension>().browser_thread();
    DCHECK_LT(id, BrowserThread::ID_COUNT);
    return id;
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForThread(
      BrowserThread::ID identifier) {
    return g_task_runners.Get().proxies[identifier];
  }
};

// |g_browser_thread_task_executor| is intentionally leaked on shutdown.
BrowserThreadTaskExecutor* g_browser_thread_task_executor = nullptr;

}  // namespace

BrowserThreadImpl::BrowserThreadImpl(
    ID identifier,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : identifier_(identifier) {
  DCHECK_GE(identifier_, 0);
  DCHECK_LT(identifier_, ID_COUNT);
  DCHECK(task_runner);

  BrowserThreadGlobals& globals = g_globals.Get();

  DCHECK_CALLED_ON_VALID_THREAD(globals.main_thread_checker_);

  DCHECK_EQ(base::subtle::NoBarrier_Load(&globals.states[identifier_]),
            BrowserThreadState::UNINITIALIZED);
  base::subtle::NoBarrier_Store(&globals.states[identifier_],
                                BrowserThreadState::RUNNING);

  DCHECK(!globals.task_runners[identifier_]);
  globals.task_runners[identifier_] = std::move(task_runner);
}

BrowserThreadImpl::~BrowserThreadImpl() {
  BrowserThreadGlobals& globals = g_globals.Get();
  DCHECK_CALLED_ON_VALID_THREAD(globals.main_thread_checker_);

  DCHECK_EQ(base::subtle::NoBarrier_Load(&globals.states[identifier_]),
            BrowserThreadState::RUNNING);
  base::subtle::NoBarrier_Store(&globals.states[identifier_],
                                BrowserThreadState::SHUTDOWN);

  // The mapping is kept alive after shutdown to avoid requiring a lock only for
  // shutdown (the SingleThreadTaskRunner itself may stop accepting tasks at any
  // point -- usually soon before/after destroying the BrowserThreadImpl).
  DCHECK(globals.task_runners[identifier_]);
}

// static
void BrowserThreadImpl::ResetGlobalsForTesting(BrowserThread::ID identifier) {
  BrowserThreadGlobals& globals = g_globals.Get();
  DCHECK_CALLED_ON_VALID_THREAD(globals.main_thread_checker_);

  DCHECK_EQ(base::subtle::NoBarrier_Load(&globals.states[identifier]),
            BrowserThreadState::SHUTDOWN);
  base::subtle::NoBarrier_Store(&globals.states[identifier],
                                BrowserThreadState::UNINITIALIZED);

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
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);

  BrowserThreadGlobals& globals = g_globals.Get();
  return base::subtle::NoBarrier_Load(&globals.states[identifier]) ==
         BrowserThreadState::RUNNING;
}

// static
bool BrowserThread::CurrentlyOn(ID identifier) {
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);

  BrowserThreadGlobals& globals = g_globals.Get();

  // Thread-safe since |globals.task_runners| is read-only after being
  // initialized from main thread (which happens before //content and embedders
  // are kicked off and enabled to call the BrowserThread API from other
  // threads).
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
  BrowserThreadGlobals& globals = g_globals.Get();

  // Thread-safe since |globals.task_runners| is read-only after being
  // initialized from main thread (which happens before //content and embedders
  // are kicked off and enabled to call the BrowserThread API from other
  // threads).
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

// static
void BrowserThreadImpl::CreateTaskExecutor() {
  DCHECK(!g_browser_thread_task_executor);
  g_browser_thread_task_executor = new BrowserThreadTaskExecutor();
  base::RegisterTaskExecutor(BrowserTaskTraitsExtension::kExtensionId,
                             g_browser_thread_task_executor);
}

// static
void BrowserThreadImpl::ResetTaskExecutorForTesting() {
  DCHECK(g_browser_thread_task_executor);
  base::UnregisterTaskExecutorForTesting(
      BrowserTaskTraitsExtension::kExtensionId);
  delete g_browser_thread_task_executor;
  g_browser_thread_task_executor = nullptr;
}

}  // namespace content
