// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/test_web_thread_bundle.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/task/task_scheduler/task_scheduler_impl.h"
#include "base/test/scoped_task_environment.h"
#include "ios/web/public/test/test_web_thread.h"
#include "ios/web/web_thread_impl.h"
#include "net/disk_cache/simple/simple_backend_impl.h"

namespace web {

TestWebThreadBundle::TestWebThreadBundle() {
  Init(TestWebThreadBundle::DEFAULT);
}

TestWebThreadBundle::TestWebThreadBundle(int options) {
  Init(options);
}

TestWebThreadBundle::~TestWebThreadBundle() {
  // To avoid memory leaks, ensure that any tasks posted to the cache pool via
  // PostTaskAndReply are able to reply back to the originating thread, by
  // flushing the cache pool while the browser threads still exist.
  base::RunLoop().RunUntilIdle();
  disk_cache::SimpleBackendImpl::FlushWorkerPoolForTesting();

  // To ensure a clean teardown, each thread's message loop must be flushed
  // just before the thread is destroyed. But destroying a fake thread does not
  // automatically flush the message loop, so do it manually.
  // See http://crbug.com/247525 for discussion.
  base::RunLoop().RunUntilIdle();
  io_thread_.reset();
  base::RunLoop().RunUntilIdle();
  // This is the point at which the cache pool is normally shut down. So flush
  // it again in case any shutdown tasks have been posted to the pool from the
  // threads above.
  disk_cache::SimpleBackendImpl::FlushWorkerPoolForTesting();
  base::RunLoop().RunUntilIdle();
  ui_thread_.reset();
  base::RunLoop().RunUntilIdle();

  message_loop_.reset();

  base::TaskScheduler::GetInstance()->FlushForTesting();
  base::TaskScheduler::GetInstance()->Shutdown();
  base::TaskScheduler::GetInstance()->JoinForTesting();
  base::TaskScheduler::SetInstance(nullptr);

  WebThreadImpl::ResetTaskExecutorForTesting();
}

void TestWebThreadBundle::Init(int options) {
  WebThreadImpl::CreateTaskExecutor();

  // TODO(crbug.com/903803): Use ScopedTaskEnviroment here instead.
  message_loop_ = std::make_unique<base::MessageLoop>(
      options & TestWebThreadBundle::IO_MAINLOOP ? base::MessageLoop::TYPE_IO
                                                 : base::MessageLoop::TYPE_UI);

  // TODO(crbug.com/826465): TestWebThread won't need MessageLoop*
  // once modernized to match its //content equivalent.
  ui_thread_.reset(new TestWebThread(WebThread::UI, message_loop_.get()));

  if (options & TestWebThreadBundle::REAL_IO_THREAD) {
    io_thread_.reset(new TestWebThread(WebThread::IO));
    io_thread_->StartIOThread();
  } else {
    io_thread_.reset(new TestWebThread(WebThread::IO, message_loop_.get()));
  }

  // Copied from ScopedTaskEnvironment::ScopedTaskEnvironment.
  // TODO(crbug.com/821034): Bring ScopedTaskEnvironment back in
  // TestWebThreadBundle.
  constexpr int kMaxThreads = 2;
  const base::TimeDelta kSuggestedReclaimTime = base::TimeDelta::Max();
  const base::SchedulerWorkerPoolParams worker_pool_params(
      kMaxThreads, kSuggestedReclaimTime);
  base::TaskScheduler::SetInstance(
      std::make_unique<base::internal::TaskSchedulerImpl>(
          "ScopedTaskEnvironment"));
  base::TaskScheduler::GetInstance()->Start(
      {worker_pool_params, worker_pool_params, worker_pool_params,
       worker_pool_params});
}

}  // namespace web
