// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_task_executor.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "content/browser/browser_thread_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class BrowserTaskExecutorTest : public testing::Test {
 public:
  void SetUp() override {
    base::TaskScheduler::CreateAndStartWithDefaultParams("Test");
    BrowserTaskExecutor::Create();
    message_loop_ = std::make_unique<base::MessageLoop>();
  }

  void TearDown() override {
    BrowserTaskExecutor::ResetForTesting();
    base::TaskScheduler::GetInstance()->Shutdown();
    base::TaskScheduler::GetInstance()->JoinForTesting();
    base::TaskScheduler::SetInstance(nullptr);
    message_loop_.reset();
  }

  static void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

 protected:
  std::unique_ptr<base::MessageLoop> message_loop_;
};

TEST_F(BrowserTaskExecutorTest, EnsureUIThreadTraitPointsToExpectedQueue) {
  EXPECT_EQ(base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}),
            BrowserThreadImpl::GetTaskRunnerForThread(BrowserThread::UI));
}

TEST_F(BrowserTaskExecutorTest, EnsureIOThreadTraitPointsToExpectedQueue) {
  EXPECT_EQ(base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
            BrowserThreadImpl::GetTaskRunnerForThread(BrowserThread::IO));
}

}  // namespace content
