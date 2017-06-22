// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_thread_bundle.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/test/scoped_async_task_scheduler.h"
#include "content/browser/browser_thread_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace content {

TestBrowserThreadBundle::TestBrowserThreadBundle()
    : TestBrowserThreadBundle(DEFAULT) {}

TestBrowserThreadBundle::TestBrowserThreadBundle(int options)
    : options_(options), threads_created_(false) {
  Init();
}

TestBrowserThreadBundle::~TestBrowserThreadBundle() {
  CHECK(threads_created_);

  // To avoid memory leaks, we must ensure that any tasks posted to the blocking
  // pool via PostTaskAndReply are able to reply back to the originating thread.
  // Thus we must flush the blocking pool while the browser threads still exist.
  base::RunLoop().RunUntilIdle();
  BrowserThreadImpl::FlushThreadPoolHelperForTesting();

  // To ensure a clean teardown, each thread's message loop must be flushed
  // just before the thread is destroyed. But stopping a fake thread does not
  // automatically flush the message loop, so we have to do it manually.
  // See http://crbug.com/247525 for discussion.
  base::RunLoop().RunUntilIdle();
  io_thread_->Stop();
  base::RunLoop().RunUntilIdle();
  cache_thread_->Stop();
  base::RunLoop().RunUntilIdle();
  process_launcher_thread_->Stop();
  base::RunLoop().RunUntilIdle();
  file_user_blocking_thread_->Stop();
  base::RunLoop().RunUntilIdle();
  file_thread_->Stop();
  base::RunLoop().RunUntilIdle();
  db_thread_->Stop();
  base::RunLoop().RunUntilIdle();
  // This is the point at which we normally shut down the thread pool. So flush
  // it again in case any shutdown tasks have been posted to the pool from the
  // threads above.
  BrowserThreadImpl::FlushThreadPoolHelperForTesting();
  base::RunLoop().RunUntilIdle();
  ui_thread_->Stop();
  base::RunLoop().RunUntilIdle();

  // This is required to ensure we run all remaining tasks in an atomic step
  // (instead of ~ScopedAsyncTaskScheduler() followed by another
  // RunLoop().RunUntilIdle()). Otherwise If a pending task in
  // |scoped_async_task_scheduler_| posts to |message_loop_|, that task can then
  // post back to |scoped_async_task_scheduler_| after the former was destroyed.
  // This is a bit different than production where the main thread is not
  // flushed after it's done running but this approach is preferred in unit
  // tests as running more tasks can merely uncover more issues (e.g. if a bad
  // tasks is posted but never blocked upon it could make a test flaky whereas
  // by flushing we guarantee it will blow up).
  RunAllBlockingPoolTasksUntilIdle();

  scoped_async_task_scheduler_.reset();
  CHECK(base::MessageLoop::current()->IsIdleForTesting());

  // |message_loop_| needs to explicitly go away before fake threads in order
  // for DestructionObservers hooked to |message_loop_| to be able to invoke
  // BrowserThread::CurrentlyOn() -- ref. ~TestBrowserThread().
  message_loop_.reset();

#if defined(OS_WIN)
  com_initializer_.reset();
#endif
}

void TestBrowserThreadBundle::Init() {
  // Check that the UI thread hasn't already been initialized. This will fail if
  // multiple TestBrowserThreadBundles are initialized in the same scope.
  CHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI));

  // Check for conflicting options can't have two IO threads.
  CHECK(!(options_ & IO_MAINLOOP) || !(options_ & REAL_IO_THREAD));
  // There must be a thread to start to use DONT_CREATE_THREADS
  CHECK((options_ & ~IO_MAINLOOP) != DONT_CREATE_THREADS);

#if defined(OS_WIN)
  // Similar to Chrome's UI thread, we need to initialize COM separately for
  // this thread as we don't call Start() for the UI TestBrowserThread; it's
  // already started!
  com_initializer_ = base::MakeUnique<base::win::ScopedCOMInitializer>();
  CHECK(com_initializer_->succeeded());
#endif

  // Create the main MessageLoop, if it doesn't already exist, and set the
  // current thread as the UI thread. In production, this work is done in
  // BrowserMainLoop::MainMessageLoopStart(). The main MessageLoop may already
  // exist if this TestBrowserThreadBundle is instantiated in a test whose
  // parent fixture provides a base::test::ScopedTaskEnvironment.
  const base::MessageLoop::Type message_loop_type =
      options_ & IO_MAINLOOP ? base::MessageLoop::TYPE_IO
                             : base::MessageLoop::TYPE_UI;
  if (!base::MessageLoop::current())
    message_loop_ = base::MakeUnique<base::MessageLoop>(message_loop_type);
  CHECK(base::MessageLoop::current()->IsType(message_loop_type));

  ui_thread_ = base::MakeUnique<TestBrowserThread>(
      BrowserThread::UI, base::MessageLoop::current());

  if (!(options_ & DONT_CREATE_THREADS))
    CreateThreads();
}

// This method mimics the work done in BrowserMainLoop::CreateThreads().
void TestBrowserThreadBundle::CreateThreads() {
  CHECK(!threads_created_);

  // TaskScheduler can sometimes be externally provided by a
  // base::test::ScopedTaskEnvironment in a parent fixture. In that case it's
  // expected to have provided the MessageLoop as well (in which case
  // |message_loop_| remains null in Init()).
  CHECK(!base::TaskScheduler::GetInstance() || !message_loop_);
  if (!base::TaskScheduler::GetInstance()) {
    scoped_async_task_scheduler_ =
        base::MakeUnique<base::test::ScopedAsyncTaskScheduler>();
  }

  if (options_ & REAL_DB_THREAD) {
    db_thread_ = base::MakeUnique<TestBrowserThread>(BrowserThread::DB);
    db_thread_->Start();
  } else {
    db_thread_ = base::MakeUnique<TestBrowserThread>(
        BrowserThread::DB, base::MessageLoop::current());
  }

  if (options_ & REAL_FILE_THREAD) {
    file_thread_ = base::MakeUnique<TestBrowserThread>(BrowserThread::FILE);
    file_thread_->Start();
  } else {
    file_thread_ = base::MakeUnique<TestBrowserThread>(
        BrowserThread::FILE, base::MessageLoop::current());
  }

  file_user_blocking_thread_ = base::MakeUnique<TestBrowserThread>(
      BrowserThread::FILE_USER_BLOCKING, base::MessageLoop::current());
  process_launcher_thread_ = base::MakeUnique<TestBrowserThread>(
      BrowserThread::PROCESS_LAUNCHER, base::MessageLoop::current());
  cache_thread_ = base::MakeUnique<TestBrowserThread>(
      BrowserThread::CACHE, base::MessageLoop::current());

  if (options_ & REAL_IO_THREAD) {
    io_thread_ = base::MakeUnique<TestBrowserThread>(BrowserThread::IO);
    io_thread_->StartIOThread();
  } else {
    io_thread_ = base::MakeUnique<TestBrowserThread>(
        BrowserThread::IO, base::MessageLoop::current());
  }

  threads_created_ = true;
}

}  // namespace content
