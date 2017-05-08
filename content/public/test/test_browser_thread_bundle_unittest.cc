// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_thread_bundle.h"

#include "base/message_loop/message_loop.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(TestBrowserThreadBundle, ScopedTaskEnvironmentAndTestBrowserThreadBundle) {
  base::test::ScopedTaskEnvironment scoped_task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::UI);
  TestBrowserThreadBundle test_browser_thread_bundle;
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
  TestBrowserThreadBundle test_browser_thread_bundle;
  EXPECT_DEATH({ TestBrowserThreadBundle other_test_browser_thread_bundle; },
               "");
}

}  // namespace content
