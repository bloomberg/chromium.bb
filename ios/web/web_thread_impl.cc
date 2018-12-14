// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_thread_impl.h"

#include <string>
#include <utility>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_executor.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread_delegate.h"

namespace web {

namespace {

// Friendly names for the well-known threads.
const char* const g_web_thread_names[WebThread::ID_COUNT] = {
    "Web_UIThread",                // UI
    "Web_IOThread",                // IO
};

static const char* GetThreadName(WebThread::ID thread) {
  if (WebThread::UI <= thread && thread < WebThread::ID_COUNT)
    return g_web_thread_names[thread];
  return "Unknown Thread";
}

// An implementation of SingleThreadTaskRunner to be used in conjunction
// with WebThread.
class WebThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit WebThreadTaskRunner(WebThread::ID identifier) : id_(identifier) {}

  // SingleThreadTaskRunner implementation.
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return base::PostDelayedTaskWithTraits(from_here, {id_}, std::move(task),
                                           delay);
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return base::PostDelayedTaskWithTraits(from_here, {id_, NonNestable()},
                                           std::move(task), delay);
  }

  bool RunsTasksInCurrentSequence() const override {
    return WebThread::CurrentlyOn(id_);
  }

 protected:
  ~WebThreadTaskRunner() override {}

 private:
  WebThread::ID id_;
  DISALLOW_COPY_AND_ASSIGN(WebThreadTaskRunner);
};

// A separate helper is used just for the task runners, in order to avoid
// needing to initialize the globals to create a task runner.
struct WebThreadTaskRunners {
  WebThreadTaskRunners() {
    for (int i = 0; i < WebThread::ID_COUNT; ++i) {
      task_runners[i] = new WebThreadTaskRunner(static_cast<WebThread::ID>(i));
    }
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runners[WebThread::ID_COUNT];
};

base::LazyInstance<WebThreadTaskRunners>::Leaky g_task_runners =
    LAZY_INSTANCE_INITIALIZER;

// State of a given WebThread::ID.
enum WebThreadState {
  // WebThread::ID does not exist.
  UNINITIALIZED = 0,
  // WebThread::ID is associated with a WebThreadImpl instance but the
  // underlying thread hasn't started yet.
  INITIALIZED,
  // WebThread::ID is associated to a TaskRunner and is accepting tasks.
  RUNNING,
  // WebThread::ID no longer accepts tasks.
  SHUTDOWN,
};

using WebThreadDelegateAtomicPtr = base::subtle::AtomicWord;

struct WebThreadGlobals {
  WebThreadGlobals() {
  }

  // This lock protects |threads| and |states|. Do not read or modify those
  // arrays without holding this lock. Do not block while holding this lock.
  base::Lock lock;

  // This array is protected by |lock|. This array is filled as WebThreadImpls
  // are constructed and depopulated when they are destructed.
  scoped_refptr<base::SingleThreadTaskRunner>
      task_runners[WebThread::ID_COUNT] GUARDED_BY(lock);

  // This array is protected by |lock|. Holds the state of each WebThread::ID.
  WebThreadState states[WebThread::ID_COUNT] GUARDED_BY(lock) = {};

  // Only atomic operations are used on this pointer. The delegate isn't owned
  // by WebThreadGlobals, rather by whoever calls
  // WebThread::SetIOThreadDelegate.
  WebThreadDelegateAtomicPtr io_thread_delegate;
};

base::LazyInstance<WebThreadGlobals>::Leaky g_globals =
    LAZY_INSTANCE_INITIALIZER;

bool PostTaskHelper(WebThread::ID identifier,
                    const base::Location& from_here,
                    base::OnceClosure task,
                    base::TimeDelta delay,
                    bool nestable) NO_THREAD_SAFETY_ANALYSIS {
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, WebThread::ID_COUNT);
  // Optimization: to avoid unnecessary locks, we listed the ID enumeration in
  // order of lifetime.  So no need to lock if we know that the target thread
  // outlives current thread.
  // Note: since the array is so small, ok to loop instead of creating a map,
  // which would require a lock because std::map isn't thread safe, defeating
  // the whole purpose of this optimization.
  WebThread::ID current_thread = WebThread::ID_COUNT;
  bool target_thread_outlives_current =
      WebThread::GetCurrentThreadIdentifier(&current_thread) &&
      current_thread >= identifier;

  WebThreadGlobals& globals = g_globals.Get();
  if (!target_thread_outlives_current)
    globals.lock.Acquire();

  const bool accepting_tasks =
      globals.states[identifier] == WebThreadState::RUNNING;
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

class WebThreadTaskExecutor : public base::TaskExecutor {
 public:
  WebThreadTaskExecutor() {}
  ~WebThreadTaskExecutor() override {}

  // base::TaskExecutor implementation.
  bool PostDelayedTaskWithTraits(const base::Location& from_here,
                                 const base::TaskTraits& traits,
                                 base::OnceClosure task,
                                 base::TimeDelta delay) override {
    return PostTaskHelper(
        GetWebThreadIdentifier(traits), from_here, std::move(task), delay,
        traits.GetExtension<WebTaskTraitsExtension>().nestable());
  }

  scoped_refptr<base::TaskRunner> CreateTaskRunnerWithTraits(
      const base::TaskTraits& traits) override {
    return GetTaskRunnerForThread(GetWebThreadIdentifier(traits));
  }

  scoped_refptr<base::SequencedTaskRunner> CreateSequencedTaskRunnerWithTraits(
      const base::TaskTraits& traits) override {
    return GetTaskRunnerForThread(GetWebThreadIdentifier(traits));
  }

  scoped_refptr<base::SingleThreadTaskRunner>
  CreateSingleThreadTaskRunnerWithTraits(
      const base::TaskTraits& traits,
      base::SingleThreadTaskRunnerThreadMode thread_mode) override {
    // It's not possible to request DEDICATED access to a WebThread.
    DCHECK_EQ(thread_mode, base::SingleThreadTaskRunnerThreadMode::SHARED);
    return GetTaskRunnerForThread(GetWebThreadIdentifier(traits));
  }

 private:
  WebThread::ID GetWebThreadIdentifier(const base::TaskTraits& traits) {
    DCHECK_EQ(traits.extension_id(), WebTaskTraitsExtension::kExtensionId);
    WebThread::ID id =
        traits.GetExtension<WebTaskTraitsExtension>().web_thread();
    DCHECK_LT(id, WebThread::ID_COUNT);

    // TODO(crbug.com/872372): Support shutdown behavior on UI/IO threads.
    if (traits.shutdown_behavior_set_explicitly()) {
      if (id == WebThread::UI) {
        DCHECK_EQ(base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
                  traits.shutdown_behavior())
            << "Only SKIP_ON_SHUTDOWN is supported on UI thread.";
      } else if (id == WebThread::IO) {
        DCHECK_EQ(base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
                  traits.shutdown_behavior())
            << "Only BLOCK_SHUTDOWN is supported on IO thread.";
      }
    }

    return id;
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForThread(
      WebThread::ID identifier) {
    return g_task_runners.Get().task_runners[identifier];
  }
};

// |g_web_thread_task_executor| is intentionally leaked on shutdown.
WebThreadTaskExecutor* g_web_thread_task_executor = nullptr;

}  // namespace

WebThreadImpl::WebThreadImpl(ID identifier)
    : Thread(GetThreadName(identifier)), identifier_(identifier) {
  Initialize();
}

WebThreadImpl::WebThreadImpl(ID identifier, base::MessageLoop* message_loop)
    : Thread(GetThreadName(identifier)), identifier_(identifier) {
  SetMessageLoop(message_loop);
  Initialize();

  // If constructed with an explicit message loop, this is a fake
  // WebThread which runs on the current thread.
  WebThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);

  DCHECK(!globals.task_runners[identifier_]);
  globals.task_runners[identifier_] = this->task_runner();

  DCHECK_EQ(globals.states[identifier_], WebThreadState::INITIALIZED);
  globals.states[identifier_] = WebThreadState::RUNNING;
}

void WebThreadImpl::Init() {
  WebThreadGlobals& globals = g_globals.Get();

#if DCHECK_IS_ON()
  {
    base::AutoLock lock(globals.lock);
    // |globals| should already have been initialized for |identifier_| in
    // WebThreadImpl::StartWithOptions(). If this isn't the case it's likely
    // because this WebThreadImpl's owner incorrectly used Thread::Start.*()
    // instead of WebThreadImpl::Start.*().
    DCHECK_EQ(globals.states[identifier_], WebThreadState::RUNNING);
    DCHECK(globals.task_runners[identifier_]);
    DCHECK(globals.task_runners[identifier_]->RunsTasksInCurrentSequence());
  }
#endif  // DCHECK_IS_ON()

  if (identifier_ == WebThread::IO) {
    WebThreadDelegateAtomicPtr delegate =
        base::subtle::NoBarrier_Load(&globals.io_thread_delegate);
    if (delegate)
      reinterpret_cast<WebThreadDelegate*>(delegate)->Init();
  }
}

NOINLINE void WebThreadImpl::UIThreadRun(base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void WebThreadImpl::IOThreadRun(base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

bool WebThreadImpl::Start() {
  return StartWithOptions(base::Thread::Options());
}

bool WebThreadImpl::StartWithOptions(const Options& options) {
  WebThreadGlobals& globals = g_globals.Get();

  // Holding the lock is necessary when kicking off the thread to ensure
  // |states| and |task_runners| are updated before it gets to query them.
  base::AutoLock lock(globals.lock);

  bool result = Thread::StartWithOptions(options);

  // Although the thread is starting asynchronously, the MessageLoop is already
  // ready to accept tasks and as such this BrowserThreadImpl is considered as
  // "running".
  DCHECK_GE(identifier_, 0);
  DCHECK_LT(identifier_, ID_COUNT);
  DCHECK(!globals.task_runners[identifier_]);
  globals.task_runners[identifier_] = this->task_runner();
  DCHECK(globals.task_runners[identifier_]);

  DCHECK_EQ(globals.states[identifier_], WebThreadState::INITIALIZED);
  globals.states[identifier_] = WebThreadState::RUNNING;

  return result;
}

bool WebThreadImpl::StartAndWaitForTesting() {
  if (!Start())
    return false;
  WaitUntilThreadStarted();
  return true;
}

void WebThreadImpl::Run(base::RunLoop* run_loop) {
  WebThread::ID thread_id = ID_COUNT;
  if (!GetCurrentThreadIdentifier(&thread_id))
    return Thread::Run(run_loop);

  switch (thread_id) {
    case WebThread::UI:
      return UIThreadRun(run_loop);
    case WebThread::IO:
      return IOThreadRun(run_loop);
    case WebThread::ID_COUNT:
      CHECK(false);  // This shouldn't actually be reached!
      break;
  }
  Thread::Run(run_loop);
}

void WebThreadImpl::CleanUp() {
  WebThreadGlobals& globals = g_globals.Get();

  if (identifier_ == WebThread::IO) {
    WebThreadDelegateAtomicPtr delegate =
        base::subtle::NoBarrier_Load(&globals.io_thread_delegate);
    if (delegate)
      reinterpret_cast<WebThreadDelegate*>(delegate)->CleanUp();
  }

  // Change the state to SHUTDOWN so that PostTaskHelper stops accepting tasks
  // for this thread. Do not clear globals.task_runners[identifier_] so that
  // WebThread::CurrentlyOn() works from the MessageLoop's
  // DestructionObservers.
  base::AutoLock lock(globals.lock);
  DCHECK_EQ(globals.states[identifier_], WebThreadState::RUNNING);
  globals.states[identifier_] = WebThreadState::SHUTDOWN;
}

void WebThreadImpl::Initialize() {
  WebThreadGlobals& globals = g_globals.Get();

  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier_, 0);
  DCHECK_LT(identifier_, ID_COUNT);
  DCHECK_EQ(globals.states[identifier_], WebThreadState::UNINITIALIZED);
  DCHECK(globals.task_runners[identifier_] == nullptr);
  globals.states[identifier_] = WebThreadState::INITIALIZED;
}

// static
void WebThreadImpl::ResetGlobalsForTesting(WebThread::ID identifier) {
  WebThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK_EQ(globals.states[identifier], WebThreadState::SHUTDOWN);
  globals.states[identifier] = WebThreadState::UNINITIALIZED;
  globals.task_runners[identifier] = nullptr;
  if (identifier == WebThread::IO)
    SetIOThreadDelegate(nullptr);
}

WebThreadImpl::~WebThreadImpl() {
  // All Thread subclasses must call Stop() in the destructor. This is
  // doubly important here as various bits of code check they are on
  // the right WebThread.
  Stop();

  WebThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  // This thread should have gone through Cleanup() as part of Stop() and be in
  // the SHUTDOWN state already (unless it uses an externally provided
  // MessageLoop instead of a real underlying thread and thus doesn't go through
  // Cleanup()).
  if (using_external_message_loop()) {
    DCHECK_EQ(globals.states[identifier_], WebThreadState::RUNNING);
    globals.states[identifier_] = WebThreadState::SHUTDOWN;
  } else {
    DCHECK_EQ(globals.states[identifier_], WebThreadState::SHUTDOWN);
  }
  globals.task_runners[identifier_] = nullptr;
#if DCHECK_IS_ON()
  // Double check that the threads are ordered correctly in the enumeration.
  for (int i = identifier_ + 1; i < ID_COUNT; ++i) {
    DCHECK(globals.states[i] == WebThreadState::SHUTDOWN ||
           globals.states[i] == WebThreadState::UNINITIALIZED)
        << "Threads must be listed in the reverse order that they die";
    DCHECK(!globals.task_runners[i])
        << "Threads must be listed in the reverse order that they die";
  }
#endif  // DCHECK_IS_ON()
}

// static
bool WebThread::IsThreadInitialized(ID identifier) {
  if (!g_globals.IsCreated())
    return false;

  WebThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  return globals.states[identifier] == WebThreadState::INITIALIZED ||
         globals.states[identifier] == WebThreadState::RUNNING;
}

// static
bool WebThread::CurrentlyOn(ID identifier) {
  WebThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  return globals.task_runners[identifier] &&
         globals.task_runners[identifier]->BelongsToCurrentThread();
}

// static
std::string WebThread::GetDCheckCurrentlyOnErrorMessage(ID expected) {
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
bool WebThread::GetCurrentThreadIdentifier(ID* identifier) {
  if (!g_globals.IsCreated())
    return false;

  WebThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  for (int i = 0; i < ID_COUNT; ++i) {
    if (globals.task_runners[i] &&
        globals.task_runners[i]->BelongsToCurrentThread()) {
      *identifier = static_cast<ID>(i);
      return true;
    }
  }

  return false;
}

// static
scoped_refptr<base::SingleThreadTaskRunner> WebThread::GetTaskRunnerForThread(
    ID identifier) {
  return g_task_runners.Get().task_runners[identifier];
}

// static
void WebThread::SetIOThreadDelegate(WebThreadDelegate* delegate) {
  using base::subtle::AtomicWord;
  WebThreadGlobals& globals = g_globals.Get();
  WebThreadDelegateAtomicPtr old_delegate =
      base::subtle::NoBarrier_AtomicExchange(
          &globals.io_thread_delegate,
          reinterpret_cast<WebThreadDelegateAtomicPtr>(delegate));

  // This catches registration when previously registered.
  DCHECK(!delegate || !old_delegate);
}

// static
void WebThreadImpl::CreateTaskExecutor() {
  DCHECK(!g_web_thread_task_executor);
  g_web_thread_task_executor = new WebThreadTaskExecutor();
  base::RegisterTaskExecutor(WebTaskTraitsExtension::kExtensionId,
                             g_web_thread_task_executor);
}

// static
void WebThreadImpl::ResetTaskExecutorForTesting() {
  DCHECK(g_web_thread_task_executor);
  base::UnregisterTaskExecutorForTesting(WebTaskTraitsExtension::kExtensionId);
  delete g_web_thread_task_executor;
  g_web_thread_task_executor = nullptr;
}

}  // namespace web
