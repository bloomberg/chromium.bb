// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_thread_bundle.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/public/test/test_browser_thread.h"

namespace content {

TestBrowserThreadBundle::TestBrowserThreadBundle()
    : TestBrowserThreadBundle(DEFAULT) {}

TestBrowserThreadBundle::TestBrowserThreadBundle(int options)
    : options_(options), threads_started_(false) {
  Init();
}

TestBrowserThreadBundle::~TestBrowserThreadBundle() {
  // To avoid memory leaks, we must ensure that any tasks posted to the blocking
  // pool via PostTaskAndReply are able to reply back to the originating thread.
  // Thus we must flush the blocking pool while the browser threads still exist.
  base::RunLoop().RunUntilIdle();
  BrowserThreadImpl::FlushThreadPoolHelperForTesting();

  // To ensure a clean teardown, each thread's message loop must be flushed
  // just before the thread is destroyed. But destroying a fake thread does not
  // automatically flush the message loop, so we have to do it manually.
  // See http://crbug.com/247525 for discussion.
  base::RunLoop().RunUntilIdle();
  io_thread_.reset();
  base::RunLoop().RunUntilIdle();
  cache_thread_.reset();
  base::RunLoop().RunUntilIdle();
  process_launcher_thread_.reset();
  base::RunLoop().RunUntilIdle();
  file_user_blocking_thread_.reset();
  base::RunLoop().RunUntilIdle();
  file_thread_.reset();
  base::RunLoop().RunUntilIdle();
  db_thread_.reset();
  base::RunLoop().RunUntilIdle();
  // This is the point at which we normally shut down the thread pool. So flush
  // it again in case any shutdown tasks have been posted to the pool from the
  // threads above.
  BrowserThreadImpl::FlushThreadPoolHelperForTesting();
  base::RunLoop().RunUntilIdle();
  ui_thread_.reset();
  base::RunLoop().RunUntilIdle();
}

void TestBrowserThreadBundle::Init() {
  // Check for conflicting options can't have two IO threads.
  CHECK(!(options_ & IO_MAINLOOP) || !(options_ & REAL_IO_THREAD));
  // There must be a thread to start to use DONT_START_THREADS
  CHECK((options_ & ~IO_MAINLOOP) != DONT_START_THREADS);

  if (options_ & IO_MAINLOOP) {
    message_loop_.reset(new base::MessageLoopForIO());
  } else {
    message_loop_.reset(new base::MessageLoopForUI());
  }

  ui_thread_.reset(
      new TestBrowserThread(BrowserThread::UI, message_loop_.get()));

  if (options_ & REAL_DB_THREAD) {
    db_thread_.reset(new TestBrowserThread(BrowserThread::DB));
  } else {
    db_thread_.reset(
        new TestBrowserThread(BrowserThread::DB, message_loop_.get()));
  }

  if (options_ & REAL_FILE_THREAD) {
    file_thread_.reset(new TestBrowserThread(BrowserThread::FILE));
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
  } else {
    io_thread_.reset(
        new TestBrowserThread(BrowserThread::IO, message_loop_.get()));
  }

  if (!(options_ & DONT_START_THREADS))
    Start();
}

void TestBrowserThreadBundle::Start() {
  DCHECK(!threads_started_);

  if (options_ & REAL_DB_THREAD)
    db_thread_->Start();

  if (options_ & REAL_FILE_THREAD)
    file_thread_->Start();

  if (options_ & REAL_IO_THREAD)
    io_thread_->StartIOThread();

  threads_started_ = true;
}

}  // namespace content
