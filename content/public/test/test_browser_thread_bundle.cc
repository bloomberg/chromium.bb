// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_thread_bundle.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/public/test/test_browser_thread.h"

namespace content {

TestBrowserThreadBundle::TestBrowserThreadBundle() {
  Init(DEFAULT);
}

TestBrowserThreadBundle::TestBrowserThreadBundle(int options) {
  Init(options);
}

TestBrowserThreadBundle::~TestBrowserThreadBundle() {
  // To avoid memory leaks, we must ensure that any tasks posted to the blocking
  // pool via PostTaskAndReply are able to reply back to the originating thread.
  // Thus we must flush the blocking pool while the browser threads still exist.
  base::RunLoop().RunUntilIdle();
  BrowserThreadImpl::FlushThreadPoolHelper();

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
  BrowserThreadImpl::FlushThreadPoolHelper();
  base::RunLoop().RunUntilIdle();
  ui_thread_.reset();
  base::RunLoop().RunUntilIdle();
}

void TestBrowserThreadBundle::Init(int options) {
  if (options & IO_MAINLOOP) {
    message_loop_.reset(new base::MessageLoopForIO());
  } else {
    message_loop_.reset(new base::MessageLoopForUI());
  }

  ui_thread_.reset(new TestBrowserThread(BrowserThread::UI,
                                         message_loop_.get()));

  if (options & REAL_DB_THREAD) {
    db_thread_.reset(new TestBrowserThread(BrowserThread::DB));
    db_thread_->Start();
  } else {
    db_thread_.reset(new TestBrowserThread(BrowserThread::DB,
                                           message_loop_.get()));
  }

  if (options & REAL_FILE_THREAD) {
    file_thread_.reset(new TestBrowserThread(BrowserThread::FILE));
    file_thread_->Start();
  } else {
    file_thread_.reset(new TestBrowserThread(BrowserThread::FILE,
                                             message_loop_.get()));
  }

  if (options & REAL_FILE_USER_BLOCKING_THREAD) {
    file_user_blocking_thread_.reset(
        new TestBrowserThread(BrowserThread::FILE_USER_BLOCKING));
    file_user_blocking_thread_->Start();
  } else {
    file_user_blocking_thread_.reset(
        new TestBrowserThread(BrowserThread::FILE_USER_BLOCKING,
                              message_loop_.get()));
  }

  if (options & REAL_PROCESS_LAUNCHER_THREAD) {
    process_launcher_thread_.reset(
        new TestBrowserThread(BrowserThread::PROCESS_LAUNCHER));
    process_launcher_thread_->Start();
  } else {
    process_launcher_thread_.reset(
        new TestBrowserThread(BrowserThread::PROCESS_LAUNCHER,
                              message_loop_.get()));
  }

  if (options & REAL_CACHE_THREAD) {
    cache_thread_.reset(new TestBrowserThread(BrowserThread::CACHE));
    cache_thread_->Start();
  } else {
    cache_thread_.reset(new TestBrowserThread(BrowserThread::CACHE,
                                              message_loop_.get()));
  }

  if (options & REAL_IO_THREAD) {
    io_thread_.reset(new TestBrowserThread(BrowserThread::IO));
    io_thread_->StartIOThread();
  } else {
    io_thread_.reset(
        new TestBrowserThread(BrowserThread::IO, message_loop_.get()));
  }
}

}  // namespace content
