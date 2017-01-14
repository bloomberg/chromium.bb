// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_thread_bundle.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_async_task_scheduler.h"
#include "base/test/scoped_task_scheduler.h"
#include "content/browser/browser_thread_impl.h"
#include "content/public/test/test_browser_thread.h"

namespace content {

TestBrowserThreadBundle::TestBrowserThreadBundle()
    : TestBrowserThreadBundle(DEFAULT) {}

TestBrowserThreadBundle::TestBrowserThreadBundle(int options)
    : options_(options), threads_created_(false) {
  Init();
}

TestBrowserThreadBundle::~TestBrowserThreadBundle() {
  DCHECK(threads_created_);

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

  scoped_async_task_scheduler_.reset();
  scoped_task_scheduler_.reset();

  // |message_loop_| needs to explicitly go away before fake threads in order
  // for DestructionObservers hooked to |message_loop_| to be able to invoke
  // BrowserThread::CurrentlyOn() -- ref. ~TestBrowserThread().
  CHECK(message_loop_->IsIdleForTesting());
  message_loop_.reset();
}

void TestBrowserThreadBundle::Init() {
  // Check for conflicting options can't have two IO threads.
  CHECK(!(options_ & IO_MAINLOOP) || !(options_ & REAL_IO_THREAD));
  // There must be a thread to start to use DONT_CREATE_THREADS
  CHECK((options_ & ~IO_MAINLOOP) != DONT_CREATE_THREADS);

  // Create the UI thread. In production, this work is done in
  // BrowserMainLoop::MainMessageLoopStart().
  if (options_ & IO_MAINLOOP) {
    message_loop_.reset(new base::MessageLoopForIO());
  } else {
    message_loop_.reset(new base::MessageLoopForUI());
  }

  ui_thread_.reset(
      new TestBrowserThread(BrowserThread::UI, message_loop_.get()));

  if (!(options_ & DONT_CREATE_THREADS))
    CreateThreads();
}

// This method mimics the work done in BrowserMainLoop::CreateThreads().
void TestBrowserThreadBundle::CreateThreads() {
  DCHECK(!threads_created_);

  if (options_ & REAL_TASK_SCHEDULER) {
    scoped_async_task_scheduler_ =
        base::MakeUnique<base::test::ScopedAsyncTaskScheduler>();
  } else {
    scoped_task_scheduler_ =
        base::MakeUnique<base::test::ScopedTaskScheduler>(message_loop_.get());
  }

  if (options_ & REAL_DB_THREAD) {
    db_thread_.reset(new TestBrowserThread(BrowserThread::DB));
    db_thread_->Start();
  } else {
    db_thread_.reset(
        new TestBrowserThread(BrowserThread::DB, message_loop_.get()));
  }

  if (options_ & REAL_FILE_THREAD) {
    file_thread_.reset(new TestBrowserThread(BrowserThread::FILE));
    file_thread_->Start();
  } else {
    file_thread_.reset(
        new TestBrowserThread(BrowserThread::FILE, message_loop_.get()));
  }

  file_user_blocking_thread_.reset(new TestBrowserThread(
      BrowserThread::FILE_USER_BLOCKING, message_loop_.get()));
  process_launcher_thread_.reset(new TestBrowserThread(
      BrowserThread::PROCESS_LAUNCHER, message_loop_.get()));
  cache_thread_.reset(
      new TestBrowserThread(BrowserThread::CACHE, message_loop_.get()));

  if (options_ & REAL_IO_THREAD) {
    io_thread_.reset(new TestBrowserThread(BrowserThread::IO));
    io_thread_->StartIOThread();
  } else {
    io_thread_.reset(
        new TestBrowserThread(BrowserThread::IO, message_loop_.get()));
  }

  threads_created_ = true;
}

}  // namespace content
