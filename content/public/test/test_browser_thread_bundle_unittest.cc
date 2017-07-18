// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_thread_bundle.h"

#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/task_scheduler/post_task.h"
#include "base/test/scoped_task_environment.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::ScopedTaskEnvironment;

namespace content {

TEST(TestBrowserThreadBundleTest,
     ScopedTaskEnvironmentAndTestBrowserThreadBundle) {
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::MainThreadType::UI);
  TestBrowserThreadBundle test_browser_thread_bundle;
  base::PostTaskAndReply(
      FROM_HERE, base::BindOnce(&base::DoNothing),
      base::BindOnce([]() { DCHECK_CURRENTLY_ON(BrowserThread::UI); }));
  scoped_task_environment.RunUntilIdle();
}

// Regression test to verify that ~TestBrowserThreadBundle() doesn't hang when
// the TaskScheduler is owned by a QUEUED ScopedTaskEnvironment with pending
// tasks.
TEST(TestBrowserThreadBundleTest,
     QueuedScopedTaskEnvironmentAndTestBrowserThreadBundle) {
  ScopedTaskEnvironment queued_scoped_task_environment(
      ScopedTaskEnvironment::MainThreadType::UI,
      ScopedTaskEnvironment::ExecutionMode::QUEUED);
  base::PostTask(FROM_HERE, base::BindOnce(&base::DoNothing));

  {
    TestBrowserThreadBundle test_browser_thread_bundle;
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }  // Would hang here prior to fix.
}

TEST(TestBrowserThreadBundleTest, MessageLoopTypeMismatch) {
  base::MessageLoopForUI message_loop;
  EXPECT_DEATH(
      {
        TestBrowserThreadBundle test_browser_thread_bundle(
            TestBrowserThreadBundle::IO_MAINLOOP);
      },
      "");
}

TEST(TestBrowserThreadBundleTest, MultipleTestBrowserThreadBundle) {
  EXPECT_DEATH(
      {
        TestBrowserThreadBundle test_browser_thread_bundle;
        TestBrowserThreadBundle other_test_browser_thread_bundle;
      },
      "");
}

}  // namespace content
