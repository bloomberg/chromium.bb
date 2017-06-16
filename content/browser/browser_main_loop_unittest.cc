// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main_loop.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/sys_info.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/test/scoped_command_line.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Verify that a single-process browser process has at least as many threads as
// the number of cores in its foreground pool.
TEST(BrowserMainLoopTest, CreateThreadsInSingleProcess) {
  {
    base::MessageLoop message_loop;
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        switches::kSingleProcess);
    MainFunctionParams main_function_params(
        *scoped_command_line.GetProcessCommandLine());
    BrowserMainLoop browser_main_loop(main_function_params);
    browser_main_loop.MainMessageLoopStart();
    browser_main_loop.CreateThreads();
    EXPECT_GE(base::TaskScheduler::GetInstance()
                  ->GetMaxConcurrentTasksWithTraitsDeprecated(
                      {base::TaskPriority::USER_VISIBLE}),
              base::SysInfo::NumberOfProcessors());
    browser_main_loop.ShutdownThreadsAndCleanUp();
  }
  base::TaskScheduler::GetInstance()->JoinForTesting();
  base::TaskScheduler::SetInstance(nullptr);
}

}  // namespace content
