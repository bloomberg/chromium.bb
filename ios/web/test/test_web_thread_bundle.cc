// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/test_web_thread_bundle.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "ios/web/public/test/test_web_thread.h"
#include "ios/web/web_thread_impl.h"

namespace web {

TestWebThreadBundle::TestWebThreadBundle() {
  Init(TestWebThreadBundle::DEFAULT);
}

TestWebThreadBundle::TestWebThreadBundle(int options) {
  Init(options);
}

TestWebThreadBundle::~TestWebThreadBundle() {
  // To avoid memory leaks, ensure that any tasks posted to the blocking pool
  // via PostTaskAndReply are able to reply back to the originating thread, by
  // flushing the blocking pool while the browser threads still exist.
  base::RunLoop().RunUntilIdle();
  WebThreadImpl::FlushThreadPoolHelperForTesting();

  // To ensure a clean teardown, each thread's message loop must be flushed
  // just before the thread is destroyed. But destroying a fake thread does not
  // automatically flush the message loop, so do it manually.
  // See http://crbug.com/247525 for discussion.
  base::RunLoop().RunUntilIdle();
  io_thread_.reset();
  base::RunLoop().RunUntilIdle();
  cache_thread_.reset();
  base::RunLoop().RunUntilIdle();
  file_user_blocking_thread_.reset();
  base::RunLoop().RunUntilIdle();
  file_thread_.reset();
  base::RunLoop().RunUntilIdle();
  db_thread_.reset();
  base::RunLoop().RunUntilIdle();
  // This is the point at which the thread pool is normally shut down. So flush
  // it again in case any shutdown tasks have been posted to the pool from the
  // threads above.
  WebThreadImpl::FlushThreadPoolHelperForTesting();
  base::RunLoop().RunUntilIdle();
  ui_thread_.reset();
  base::RunLoop().RunUntilIdle();
}

void TestWebThreadBundle::Init(int options) {
  if (options & TestWebThreadBundle::IO_MAINLOOP) {
    message_loop_.reset(new base::MessageLoopForIO());
  } else {
    message_loop_.reset(new base::MessageLoopForUI());
  }

  ui_thread_.reset(new TestWebThread(WebThread::UI, message_loop_.get()));

  if (options & TestWebThreadBundle::REAL_DB_THREAD) {
    db_thread_.reset(new TestWebThread(WebThread::DB));
    db_thread_->Start();
  } else {
    db_thread_.reset(new TestWebThread(WebThread::DB, message_loop_.get()));
  }

  if (options & TestWebThreadBundle::REAL_FILE_THREAD) {
    file_thread_.reset(new TestWebThread(WebThread::FILE));
    file_thread_->Start();
  } else {
    file_thread_.reset(new TestWebThread(WebThread::FILE, message_loop_.get()));
  }

  file_user_blocking_thread_.reset(
      new TestWebThread(WebThread::FILE_USER_BLOCKING, message_loop_.get()));

  cache_thread_.reset(new TestWebThread(WebThread::CACHE, message_loop_.get()));

  if (options & TestWebThreadBundle::REAL_IO_THREAD) {
    io_thread_.reset(new TestWebThread(WebThread::IO));
    io_thread_->StartIOThread();
  } else {
    io_thread_.reset(new TestWebThread(WebThread::IO, message_loop_.get()));
  }
}

}  // namespace web
