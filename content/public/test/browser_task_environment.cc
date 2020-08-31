// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_task_environment.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/after_startup_task_utils.h"
#include "content/browser/browser_process_sub_thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/scheduler/browser_io_thread_delegate.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"

#if defined(OS_ANDROID)
#include "base/android/task_scheduler/post_task_android.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace content {

class TestBrowserThread {
 public:
  // Creates and starts a TestBrowserThread with a MessagePumpForIO.
  static std::unique_ptr<TestBrowserThread> StartIOThread();

  // Constructs a TestBrowserThread "running" on |thread_runner| (no
  // |real_thread_|).
  TestBrowserThread(BrowserThread::ID identifier,
                    scoped_refptr<base::SingleThreadTaskRunner> thread_runner);

  ~TestBrowserThread();

  // Stops the thread, no-op if this is not a real thread.
  void Stop();

 private:
  explicit TestBrowserThread(
      BrowserThread::ID identifier,
      std::unique_ptr<BrowserProcessSubThread> real_thread);

  const BrowserThread::ID identifier_;

  // A real thread which represents |identifier_| when StartIOThread() is used
  // (null otherwise).
  std::unique_ptr<BrowserProcessSubThread> real_thread_;

  // Binds |identifier_| to |thread_runner| when the public constructor is used
  // (null otherwise).
  std::unique_ptr<BrowserThreadImpl> fake_thread_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserThread);
};

std::unique_ptr<TestBrowserThread> TestBrowserThread::StartIOThread() {
  auto thread = base::WrapUnique(new TestBrowserThread(
      BrowserThread::IO, BrowserTaskExecutor::CreateIOThread()));
  thread->real_thread_->RegisterAsBrowserThread();
  return thread;
}

TestBrowserThread::TestBrowserThread(
    BrowserThread::ID identifier,
    std::unique_ptr<BrowserProcessSubThread> real_thread)
    : identifier_(identifier), real_thread_(std::move(real_thread)) {}

TestBrowserThread::TestBrowserThread(
    BrowserThread::ID identifier,
    scoped_refptr<base::SingleThreadTaskRunner> thread_runner)
    : identifier_(identifier),
      fake_thread_(
          new BrowserThreadImpl(identifier_, std::move(thread_runner))) {}

TestBrowserThread::~TestBrowserThread() {
  // The upcoming BrowserThreadImpl::ResetGlobalsForTesting() call requires that
  // |identifier_| have completed its SHUTDOWN phase.
  real_thread_.reset();
  fake_thread_.reset();

  // Resets BrowserThreadImpl's globals so that |identifier_| is no longer
  // bound. This is fine since the underlying MessageLoop has already been
  // flushed and deleted above. In the case of an externally provided
  // MessageLoop however, this means that TaskRunners obtained through
  // |BrowserThreadImpl::GetTaskRunnerForThread(identifier_)| will no longer
  // recognize their BrowserThreadImpl for RunsTasksInCurrentSequence(). This
  // happens most often when such verifications are made from
  // MessageLoopCurrent::DestructionObservers. Callers that care to work around
  // that should instead use this shutdown sequence:
  //   1) TestBrowserThread::Stop()
  //   2) ~MessageLoop()
  //   3) ~TestBrowserThread()
  // (~BrowserTaskEnvironment() does this).
  BrowserThreadImpl::ResetGlobalsForTesting(identifier_);
}

void TestBrowserThread::Stop() {
  if (real_thread_)
    real_thread_->Stop();
}

BrowserTaskEnvironment::~BrowserTaskEnvironment() {
  // This is required to ensure we run all remaining MessageLoop and
  // ThreadPool tasks in an atomic step. This is a bit different than
  // production where the main thread is not flushed after it's done running
  // but this approach is preferred in unit tests as running more tasks can
  // merely uncover more issues (e.g. if a bad tasks is posted but never
  // blocked upon it could make a test flaky whereas by flushing we guarantee
  // it will blow up).
  RunUntilIdle();

  // When REAL_IO_THREAD, we need to stop the IO thread explicitly and flush
  // again.
  if (real_io_thread_) {
    io_thread_->Stop();
    RunUntilIdle();
  }

  // The only way this check can fail after RunUntilIdle() is if a test is
  // running its own base::Thread's. Such tests should make sure to coalesce
  // independent threads before this point.
  CHECK(MainThreadIsIdle()) << sequence_manager()->DescribeAllPendingTasks();

  BrowserTaskExecutor::ResetForTesting();

  // Run DestructionObservers before our fake threads go away to ensure
  // BrowserThread::CurrentlyOn() returns the results expected by the observers.
  NotifyDestructionObserversAndReleaseSequenceManager();

#if defined(OS_WIN)
  com_initializer_.reset();
#endif
}

BrowserTaskEnvironment::BrowserTaskEnvironment(
    base::test::TaskEnvironment&& task_environment,
    bool real_io_thread)
    : base::test::TaskEnvironment(std::move(task_environment)),
      real_io_thread_(real_io_thread) {
  Init();
}

void BrowserTaskEnvironment::Init() {
  // Check that the UI thread hasn't already been initialized. This will fail if
  // multiple BrowserTaskEnvironments are initialized in the same scope.
  CHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI));

  CHECK(!real_io_thread_ || !HasIOMainLoop()) << "Can't have two IO threads";

#if defined(OS_WIN)
  // Similar to Chrome's UI thread, we need to initialize COM separately for
  // this thread as we don't call Start() for the UI TestBrowserThread; it's
  // already started!
  com_initializer_ = std::make_unique<base::win::ScopedCOMInitializer>();
  CHECK(com_initializer_->Succeeded());
#endif

  auto browser_ui_thread_scheduler = BrowserUIThreadScheduler::CreateForTesting(
      sequence_manager(), GetTimeDomain());
  auto default_ui_task_runner =
      browser_ui_thread_scheduler->GetHandle()->GetDefaultTaskRunner();
  auto browser_io_thread_delegate =
      real_io_thread_
          ? std::make_unique<BrowserIOThreadDelegate>()
          : BrowserIOThreadDelegate::CreateForTesting(sequence_manager());
  browser_io_thread_delegate->SetAllowBlockingForTesting();

  BrowserTaskExecutor::CreateForTesting(std::move(browser_ui_thread_scheduler),
                                        std::move(browser_io_thread_delegate));
  BrowserTaskExecutor::BindToUIThreadForTesting();
  DeferredInitFromSubclass(std::move(default_ui_task_runner));

  if (HasIOMainLoop()) {
    CHECK(base::MessageLoopCurrentForIO::IsSet());
  } else if (main_thread_type() == MainThreadType::UI) {
    CHECK(base::MessageLoopCurrentForUI::IsSet());
  }

  // Set the current thread as the UI thread.
  ui_thread_ = std::make_unique<TestBrowserThread>(
      BrowserThread::UI, base::ThreadTaskRunnerHandle::Get());

  if (real_io_thread_) {
    io_thread_ = TestBrowserThread::StartIOThread();
  } else {
    io_thread_ = std::make_unique<TestBrowserThread>(
        BrowserThread::IO, base::ThreadTaskRunnerHandle::Get());
  }

  // Consider startup complete such that after-startup-tasks always run in
  // the scope of the test they were posted from (http://crbug.com/732018).
  SetBrowserStartupIsCompleteForTesting();
  // Some unittests check the number of pending tasks, which will include the
  // one that enables the best effort queues, so run everything pending before
  // we hand control over to the test.
  // TODO(carlscab): Maybe find a better way to not expose control tasks
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::UI);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
}

void BrowserTaskEnvironment::RunIOThreadUntilIdle() {
  // Use a RunLoop to run until idle if already on BrowserThread::IO (which is
  // the main thread unless using REAL_IO_THREAD).
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::WaitableEvent io_thread_idle(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(
                     [](base::WaitableEvent* io_thread_idle) {
                       base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed)
                           .RunUntilIdle();
                       io_thread_idle->Signal();
                     },
                     Unretained(&io_thread_idle)));
  io_thread_idle.Wait();
}

}  // namespace content
