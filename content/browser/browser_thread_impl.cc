// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_thread_impl.h"

#include <string>
#include <utility>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/profiler/scoped_tracker.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_worker_pool.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "net/disk_cache/simple/simple_backend_impl.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif

namespace content {

namespace {

// Friendly names for the well-known threads.
static const char* const g_browser_thread_names[BrowserThread::ID_COUNT] = {
  "",  // UI (name assembled in browser_main.cc).
  "Chrome_DBThread",  // DB
  "Chrome_FileThread",  // FILE
  "Chrome_FileUserBlockingThread",  // FILE_USER_BLOCKING
  "Chrome_ProcessLauncherThread",  // PROCESS_LAUNCHER
  "Chrome_CacheThread",  // CACHE
  "Chrome_IOThread",  // IO
};

static const char* GetThreadName(BrowserThread::ID thread) {
  if (BrowserThread::UI < thread && thread < BrowserThread::ID_COUNT)
    return g_browser_thread_names[thread];
  if (thread == BrowserThread::UI)
    return "Chrome_UIThread";
  return "Unknown Thread";
}

// An implementation of SingleThreadTaskRunner to be used in conjunction
// with BrowserThread.
// TODO(gab): Consider replacing this with |g_globals->task_runners| -- only
// works if none are requested before starting the threads.
class BrowserThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit BrowserThreadTaskRunner(BrowserThread::ID identifier)
      : id_(identifier) {}

  // SingleThreadTaskRunner implementation.
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override {
    return BrowserThread::PostDelayedTask(id_, from_here, task, delay);
  }

  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override {
    return BrowserThread::PostNonNestableDelayedTask(id_, from_here, task,
                                                     delay);
  }

  bool RunsTasksOnCurrentThread() const override {
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
  // BrowserThread::ID is associated with a BrowserThreadImpl instance but the
  // underlying thread hasn't started yet.
  INITIALIZED,
  // BrowserThread::ID is associated to a TaskRunner and is accepting tasks.
  RUNNING,
  // BrowserThread::ID no longer accepts tasks.
  SHUTDOWN
};

using BrowserThreadDelegateAtomicPtr = base::subtle::AtomicWord;

struct BrowserThreadGlobals {
  BrowserThreadGlobals()
      : blocking_pool(
            new base::SequencedWorkerPool(3,
                                          "BrowserBlocking",
                                          base::TaskPriority::USER_VISIBLE)) {}

  // This lock protects |task_runners| and |states|. Do not read or modify those
  // arrays without holding this lock. Do not block while holding this lock.
  base::Lock lock;

  // This array is filled either as the underlying threads start and invoke
  // Init() or in RedirectThreadIDToTaskRunner() for threads that are being
  // redirected. It is not emptied during shutdown in order to support
  // RunsTasksOnCurrentThread() until the very end.
  scoped_refptr<base::SingleThreadTaskRunner>
      task_runners[BrowserThread::ID_COUNT];

  // Holds the state of each BrowserThread::ID.
  BrowserThreadState states[BrowserThread::ID_COUNT] = {};

  // Only atomic operations are used on this pointer. The delegate isn't owned
  // by BrowserThreadGlobals, rather by whoever calls
  // BrowserThread::SetIOThreadDelegate.
  BrowserThreadDelegateAtomicPtr io_thread_delegate = 0;

  const scoped_refptr<base::SequencedWorkerPool> blocking_pool;
};

base::LazyInstance<BrowserThreadGlobals>::Leaky
    g_globals = LAZY_INSTANCE_INITIALIZER;

}  // namespace

BrowserThreadImpl::BrowserThreadImpl(ID identifier)
    : Thread(GetThreadName(identifier)), identifier_(identifier) {
  Initialize();
}

BrowserThreadImpl::BrowserThreadImpl(ID identifier,
                                     base::MessageLoop* message_loop)
    : Thread(GetThreadName(identifier)), identifier_(identifier) {
  SetMessageLoop(message_loop);
  Initialize();

  // If constructed with an explicit message loop, this is a fake
  // BrowserThread which runs on the current thread.
  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);

  DCHECK(!globals.task_runners[identifier_]);
  globals.task_runners[identifier_] = task_runner();

  DCHECK_EQ(globals.states[identifier_], BrowserThreadState::INITIALIZED);
  globals.states[identifier_] = BrowserThreadState::RUNNING;
}

// static
void BrowserThreadImpl::ShutdownThreadPool() {
  // The goal is to make it impossible for chrome to 'infinite loop' during
  // shutdown, but to reasonably expect that all BLOCKING_SHUTDOWN tasks queued
  // during shutdown get run. There's nothing particularly scientific about the
  // number chosen.
  const int kMaxNewShutdownBlockingTasks = 1000;
  BrowserThreadGlobals& globals = g_globals.Get();
  globals.blocking_pool->Shutdown(kMaxNewShutdownBlockingTasks);
}

// static
void BrowserThreadImpl::FlushThreadPoolHelperForTesting() {
  // We don't want to create a pool if none exists.
  if (g_globals == nullptr)
    return;
  g_globals.Get().blocking_pool->FlushForTesting();
  disk_cache::SimpleBackendImpl::FlushWorkerPoolForTesting();
}

void BrowserThreadImpl::Init() {
  BrowserThreadGlobals& globals = g_globals.Get();

#if DCHECK_IS_ON()
  {
    base::AutoLock lock(globals.lock);
    // |globals| should already have been initialized for |identifier_| in
    // BrowserThreadImpl::StartWithOptions(). If this isn't the case it's likely
    // because this BrowserThreadImpl's owner incorrectly used Thread::Start.*()
    // instead of BrowserThreadImpl::Start.*().
    DCHECK_EQ(globals.states[identifier_], BrowserThreadState::RUNNING);
    DCHECK(globals.task_runners[identifier_]);
    DCHECK(globals.task_runners[identifier_]->RunsTasksOnCurrentThread());
  }
#endif  // DCHECK_IS_ON()

  if (identifier_ == BrowserThread::DB ||
      identifier_ == BrowserThread::FILE ||
      identifier_ == BrowserThread::FILE_USER_BLOCKING ||
      identifier_ == BrowserThread::PROCESS_LAUNCHER ||
      identifier_ == BrowserThread::CACHE) {
    // Nesting and task observers are not allowed on redirected threads.
    message_loop()->DisallowNesting();
    message_loop()->DisallowTaskObservers();
  }

  if (identifier_ == BrowserThread::IO) {
    BrowserThreadDelegateAtomicPtr delegate =
        base::subtle::NoBarrier_Load(&globals.io_thread_delegate);
    if (delegate)
      reinterpret_cast<BrowserThreadDelegate*>(delegate)->Init();
  }
}

// We disable optimizations for this block of functions so the compiler doesn't
// merge them all together.
MSVC_DISABLE_OPTIMIZE()
MSVC_PUSH_DISABLE_WARNING(4748)

NOINLINE void BrowserThreadImpl::UIThreadRun(base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void BrowserThreadImpl::DBThreadRun(base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void BrowserThreadImpl::FileThreadRun(base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void BrowserThreadImpl::FileUserBlockingThreadRun(
    base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void BrowserThreadImpl::ProcessLauncherThreadRun(
    base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void BrowserThreadImpl::CacheThreadRun(base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void BrowserThreadImpl::IOThreadRun(base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

MSVC_POP_WARNING()
MSVC_ENABLE_OPTIMIZE();

void BrowserThreadImpl::Run(base::RunLoop* run_loop) {
#if defined(OS_ANDROID)
  // Not to reset thread name to "Thread-???" by VM, attach VM with thread name.
  // Though it may create unnecessary VM thread objects, keeping thread name
  // gives more benefit in debugging in the platform.
  if (!thread_name().empty()) {
    base::android::AttachCurrentThreadWithName(thread_name());
  }
#endif

  BrowserThread::ID thread_id = ID_COUNT;
  CHECK(GetCurrentThreadIdentifier(&thread_id));
  CHECK_EQ(identifier_, thread_id);

  switch (identifier_) {
    case BrowserThread::UI:
      return UIThreadRun(run_loop);
    case BrowserThread::DB:
      return DBThreadRun(run_loop);
    case BrowserThread::FILE:
      return FileThreadRun(run_loop);
    case BrowserThread::FILE_USER_BLOCKING:
      return FileUserBlockingThreadRun(run_loop);
    case BrowserThread::PROCESS_LAUNCHER:
      return ProcessLauncherThreadRun(run_loop);
    case BrowserThread::CACHE:
      return CacheThreadRun(run_loop);
    case BrowserThread::IO:
      return IOThreadRun(run_loop);
    case BrowserThread::ID_COUNT:
      CHECK(false);  // This shouldn't actually be reached!
      break;
  }

  // |identifier_| must be set to a valid enum value in the constructor, so it
  // should be impossible to reach here.
  CHECK(false);
}

void BrowserThreadImpl::CleanUp() {
  BrowserThreadGlobals& globals = g_globals.Get();

  if (identifier_ == BrowserThread::IO) {
    BrowserThreadDelegateAtomicPtr delegate =
        base::subtle::NoBarrier_Load(&globals.io_thread_delegate);
    if (delegate)
      reinterpret_cast<BrowserThreadDelegate*>(delegate)->CleanUp();
  }

  // Change the state to SHUTDOWN so that PostTaskHelper stops accepting tasks
  // for this thread. Do not clear globals.task_runners[identifier_] so that
  // BrowserThread::CurrentlyOn() works from the MessageLoop's
  // DestructionObservers.
  base::AutoLock lock(globals.lock);
  DCHECK_EQ(globals.states[identifier_], BrowserThreadState::RUNNING);
  globals.states[identifier_] = BrowserThreadState::SHUTDOWN;
}

void BrowserThreadImpl::Initialize() {
  BrowserThreadGlobals& globals = g_globals.Get();

  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier_, 0);
  DCHECK_LT(identifier_, ID_COUNT);
  DCHECK_EQ(globals.states[identifier_], BrowserThreadState::UNINITIALIZED);
  globals.states[identifier_] = BrowserThreadState::INITIALIZED;
}

// static
void BrowserThreadImpl::ResetGlobalsForTesting(BrowserThread::ID identifier) {
  BrowserThreadGlobals& globals = g_globals.Get();

  base::AutoLock lock(globals.lock);
  DCHECK_EQ(globals.states[identifier], BrowserThreadState::SHUTDOWN);
  globals.states[identifier] = BrowserThreadState::UNINITIALIZED;
  globals.task_runners[identifier] = nullptr;
  if (identifier == BrowserThread::IO)
    SetIOThreadDelegate(nullptr);
}

BrowserThreadImpl::~BrowserThreadImpl() {
  // All Thread subclasses must call Stop() in the destructor. This is
  // doubly important here as various bits of code check they are on
  // the right BrowserThread.
  Stop();

  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  // This thread should have gone through Cleanup() as part of Stop() and be in
  // the SHUTDOWN state already (unless it uses an externally provided
  // MessageLoop instead of a real underlying thread and thus doesn't go through
  // Cleanup()).
  if (using_external_message_loop()) {
    DCHECK_EQ(globals.states[identifier_], BrowserThreadState::RUNNING);
    globals.states[identifier_] = BrowserThreadState::SHUTDOWN;
  } else {
    DCHECK_EQ(globals.states[identifier_], BrowserThreadState::SHUTDOWN);
  }
#if DCHECK_IS_ON()
  // Double check that the threads are ordered correctly in the enumeration.
  for (int i = identifier_ + 1; i < ID_COUNT; ++i) {
    DCHECK(globals.states[i] == BrowserThreadState::SHUTDOWN ||
           globals.states[i] == BrowserThreadState::UNINITIALIZED)
        << "Threads must be listed in the reverse order that they die";
  }
#endif
}

bool BrowserThreadImpl::Start() {
  return StartWithOptions(base::Thread::Options());
}

bool BrowserThreadImpl::StartWithOptions(const Options& options) {
  BrowserThreadGlobals& globals = g_globals.Get();

  // Holding the lock is necessary when kicking off the thread to ensure
  // |states| and |task_runners| are updated before it gets to query them.
  base::AutoLock lock(globals.lock);

  bool result = Thread::StartWithOptions(options);

  // Although the thread is starting asynchronously, the MessageLoop is already
  // ready to accept tasks and as such this BrowserThreadImpl is considered as
  // "running".
  DCHECK(!globals.task_runners[identifier_]);
  globals.task_runners[identifier_] = task_runner();
  DCHECK(globals.task_runners[identifier_]);

  DCHECK_EQ(globals.states[identifier_], BrowserThreadState::INITIALIZED);
  globals.states[identifier_] = BrowserThreadState::RUNNING;

  return result;
}

bool BrowserThreadImpl::StartAndWaitForTesting() {
  if (!Start())
    return false;
  WaitUntilThreadStarted();
  return true;
}

// static
void BrowserThreadImpl::RedirectThreadIDToTaskRunner(
    BrowserThread::ID identifier,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner);

  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);

  DCHECK(!globals.task_runners[identifier]);
  DCHECK_EQ(globals.states[identifier], BrowserThreadState::UNINITIALIZED);

  globals.task_runners[identifier] = std::move(task_runner);
  globals.states[identifier] = BrowserThreadState::RUNNING;
}

// static
void BrowserThreadImpl::StopRedirectionOfThreadID(
    BrowserThread::ID identifier) {
  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock auto_lock(globals.lock);

  DCHECK(globals.task_runners[identifier]);

  // Change the state to SHUTDOWN to stop accepting new tasks. Note: this is
  // different from non-redirected threads which continue accepting tasks while
  // being joined and only quit when idle. However, any tasks for which this
  // difference matters was already racy as any thread posting a task after the
  // Signal task below can't be synchronized with the joining thread. Therefore,
  // that task could already come in before or after the join had completed in
  // the non-redirection world. Entering SHUTDOWN early merely skews this race
  // towards making it less likely such a task is accepted by the joined thread
  // which is fine.
  DCHECK_EQ(globals.states[identifier], BrowserThreadState::RUNNING);
  globals.states[identifier] = BrowserThreadState::SHUTDOWN;

  // Wait for all pending tasks to complete.
  base::WaitableEvent flushed(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
  globals.task_runners[identifier]->PostTask(
      FROM_HERE,
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&flushed)));
  {
    base::AutoUnlock auto_lock(globals.lock);
    flushed.Wait();
  }

  // Only reset the task runner after running pending tasks so that
  // BrowserThread::CurrentlyOn() works in their scope.
  globals.task_runners[identifier] = nullptr;

  // Note: it's still possible for tasks to be posted to that task runner after
  // this point (e.g. through a previously obtained ThreadTaskRunnerHandle or by
  // one of the last tasks re-posting to its ThreadTaskRunnerHandle) but the
  // BrowserThread API itself won't accept tasks. Such tasks are ultimately
  // guaranteed to run before TaskScheduler::Shutdown() returns but may break
  // the assumption in PostTaskHelper that BrowserThread::ID A > B will always
  // succeed to post to B. This is pretty much the only observable difference
  // between a redirected thread and a real one and is one we're willing to live
  // with for this experiment. TODO(gab): fix this before enabling the
  // experiment by default on trunk, http://crbug.com/653916.
}

// static
bool BrowserThreadImpl::PostTaskHelper(
    BrowserThread::ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay,
    bool nestable) {
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  // Optimization: to avoid unnecessary locks, we listed the ID enumeration in
  // order of lifetime.  So no need to lock if we know that the target thread
  // outlives current thread as that implies the current thread only ever sees
  // the target thread in its RUNNING state.
  // Note: since the array is so small, ok to loop instead of creating a map,
  // which would require a lock because std::map isn't thread safe, defeating
  // the whole purpose of this optimization.
  BrowserThread::ID current_thread = ID_COUNT;
  bool target_thread_outlives_current =
      GetCurrentThreadIdentifier(&current_thread) &&
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
      task_runner->PostDelayedTask(from_here, task, delay);
    } else {
      task_runner->PostNonNestableDelayedTask(from_here, task, delay);
    }
  }

  if (!target_thread_outlives_current)
    globals.lock.Release();

  return accepting_tasks;
}

// static
bool BrowserThread::PostBlockingPoolTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return g_globals.Get().blocking_pool->PostWorkerTask(from_here, task);
}

// static
bool BrowserThread::PostBlockingPoolTaskAndReply(
    const tracked_objects::Location& from_here,
    base::Closure task,
    base::Closure reply) {
  return g_globals.Get().blocking_pool->PostTaskAndReply(
      from_here, std::move(task), std::move(reply));
}

// static
bool BrowserThread::PostBlockingPoolSequencedTask(
    const std::string& sequence_token_name,
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return g_globals.Get().blocking_pool->PostNamedSequencedWorkerTask(
      sequence_token_name, from_here, task);
}

// static
void BrowserThread::PostAfterStartupTask(
    const tracked_objects::Location& from_here,
    const scoped_refptr<base::TaskRunner>& task_runner,
    const base::Closure& task) {
  GetContentClient()->browser()->PostAfterStartupTask(from_here, task_runner,
                                                      task);
}

// static
base::SequencedWorkerPool* BrowserThread::GetBlockingPool() {
  return g_globals.Get().blocking_pool.get();
}

// static
bool BrowserThread::IsThreadInitialized(ID identifier) {
  if (g_globals == nullptr)
    return false;

  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  return globals.states[identifier] == BrowserThreadState::INITIALIZED ||
         globals.states[identifier] == BrowserThreadState::RUNNING;
}

// static
bool BrowserThread::CurrentlyOn(ID identifier) {
  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  return globals.task_runners[identifier] &&
         globals.task_runners[identifier]->RunsTasksOnCurrentThread();
}

// static
std::string BrowserThread::GetDCheckCurrentlyOnErrorMessage(ID expected) {
  std::string actual_name = base::PlatformThread::GetName();
  if (actual_name.empty())
    actual_name = "Unknown Thread";

  std::string result = "Must be called on ";
  result += GetThreadName(expected);
  result += "; actually called on ";
  result += actual_name;
  result += ".";
  return result;
}

// static
bool BrowserThread::IsMessageLoopValid(ID identifier) {
  if (g_globals == nullptr)
    return false;

  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  return globals.states[identifier] == BrowserThreadState::RUNNING;
}

// static
bool BrowserThread::PostTask(ID identifier,
                             const tracked_objects::Location& from_here,
                             const base::Closure& task) {
  return BrowserThreadImpl::PostTaskHelper(
      identifier, from_here, task, base::TimeDelta(), true);
}

// static
bool BrowserThread::PostDelayedTask(ID identifier,
                                    const tracked_objects::Location& from_here,
                                    const base::Closure& task,
                                    base::TimeDelta delay) {
  return BrowserThreadImpl::PostTaskHelper(
      identifier, from_here, task, delay, true);
}

// static
bool BrowserThread::PostNonNestableTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return BrowserThreadImpl::PostTaskHelper(
      identifier, from_here, task, base::TimeDelta(), false);
}

// static
bool BrowserThread::PostNonNestableDelayedTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return BrowserThreadImpl::PostTaskHelper(
      identifier, from_here, task, delay, false);
}

// static
bool BrowserThread::PostTaskAndReply(ID identifier,
                                     const tracked_objects::Location& from_here,
                                     base::Closure task,
                                     base::Closure reply) {
  return GetTaskRunnerForThread(identifier)
      ->PostTaskAndReply(from_here, std::move(task), std::move(reply));
}

// static
bool BrowserThread::GetCurrentThreadIdentifier(ID* identifier) {
  if (g_globals == nullptr)
    return false;

  BrowserThreadGlobals& globals = g_globals.Get();
  // Profiler to track potential contention on |globals.lock|. This only does
  // real work on canary and local dev builds, so the cost of having this here
  // should be minimal.
  tracked_objects::ScopedTracker tracking_profile(FROM_HERE);
  base::AutoLock lock(globals.lock);
  for (int i = 0; i < ID_COUNT; ++i) {
    if (globals.task_runners[i] &&
        globals.task_runners[i]->RunsTasksOnCurrentThread()) {
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
void BrowserThread::SetIOThreadDelegate(BrowserThreadDelegate* delegate) {
  BrowserThreadGlobals& globals = g_globals.Get();
  BrowserThreadDelegateAtomicPtr old_delegate =
      base::subtle::NoBarrier_AtomicExchange(
          &globals.io_thread_delegate,
          reinterpret_cast<BrowserThreadDelegateAtomicPtr>(delegate));

  // This catches registration when previously registered.
  DCHECK(!delegate || !old_delegate);
}

}  // namespace content
