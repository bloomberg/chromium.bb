// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_ui_thread_scheduler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool.h"
#include "base/test/mock_callback.h"
#include "content/public/browser/browser_task_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

base::OnceClosure RunOnDestruction(base::OnceClosure task) {
  return base::BindOnce(
      [](std::unique_ptr<base::ScopedClosureRunner>) {},
      base::Passed(
          std::make_unique<base::ScopedClosureRunner>(std::move(task))));
}

base::OnceClosure PostOnDestruction(
    scoped_refptr<base::SingleThreadTaskRunner> task_queue,
    base::OnceClosure task) {
  return RunOnDestruction(base::BindOnce(
      [](base::OnceClosure task,
         scoped_refptr<base::SingleThreadTaskRunner> task_queue) {
        task_queue->PostTask(FROM_HERE, std::move(task));
      },
      base::Passed(std::move(task)), task_queue));
}

TEST(BrowserUIThreadSchedulerTest, DestructorPostChainDuringShutdown) {
  auto browser_ui_thread_scheduler_ =
      std::make_unique<BrowserUIThreadScheduler>();
  auto task_queue = browser_ui_thread_scheduler_->GetHandle().task_runner(
      BrowserUIThreadScheduler::QueueType::kDefault);

  bool run = false;
  task_queue->PostTask(
      FROM_HERE,
      PostOnDestruction(
          task_queue,
          PostOnDestruction(task_queue,
                            RunOnDestruction(base::BindOnce(
                                [](bool* run) { *run = true; }, &run)))));

  EXPECT_FALSE(run);
  browser_ui_thread_scheduler_.reset();

  EXPECT_TRUE(run);
}

}  // namespace

}  // namespace content
