// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/tracing/common/trace_startup_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "services/tracing/public/cpp/trace_startup.h"

namespace content {

namespace {

// Wait until |condition| returns true.
void WaitForCondition(base::RepeatingCallback<bool()> condition,
                      const std::string& description) {
  const base::TimeDelta kTimeout = base::TimeDelta::FromSeconds(30);
  const base::TimeTicks start_time = base::TimeTicks::Now();
  while (!condition.Run() && (base::TimeTicks::Now() - start_time < kTimeout)) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
  ASSERT_TRUE(condition.Run())
      << "Timeout waiting for condition: " << description;
}

}  // namespace

class StartupTracingControllerTest : public ContentBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CreateTemporaryFile(&temp_file_path_);
    command_line->AppendSwitch(switches::kTraceStartup);
    command_line->AppendSwitchASCII(switches::kTraceStartupDuration, "3");
    command_line->AppendSwitchASCII(switches::kTraceStartupFile,
                                    temp_file_path_.AsUTF8Unsafe());

#if defined(OS_ANDROID)
    // On Android the startup tracing is initialized as soon as library load
    // time, earlier than this point. So, reset the config and enable startup
    // tracing here.
    tracing::TraceStartupConfig::GetInstance()->EnableFromCommandLine();
    tracing::EnableStartupTracingIfNeeded();
#endif
  }

 protected:
  base::FilePath temp_file_path_;
};

IN_PROC_BROWSER_TEST_F(StartupTracingControllerTest, TestStartupTracing) {
  NavigateToURL(shell(), GetTestUrl("", "title.html"));
  WaitForCondition(base::BindRepeating([]() {
                     return !TracingController::GetInstance()->IsTracing();
                   }),
                   "trace end");
  EXPECT_FALSE(tracing::TraceStartupConfig::GetInstance()->IsEnabled());
  EXPECT_FALSE(TracingController::GetInstance()->IsTracing());
  WaitForCondition(base::BindRepeating([]() {
                     return tracing::TraceStartupConfig::GetInstance()
                         ->finished_writing_to_file_for_testing();
                   }),
                   "finish file write");

  std::string trace;
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::ReadFileToString(temp_file_path_, &trace));
  EXPECT_TRUE(trace.find("BrowserMainLoop::InitStartupTracingForDuration") !=
              std::string::npos);
}

}  // namespace content
