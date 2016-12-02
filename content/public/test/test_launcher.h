// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_LAUNCHER_H_
#define CONTENT_PUBLIC_TEST_TEST_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/test/launcher/test_launcher.h"

namespace base {
class CommandLine;
class FilePath;
}

namespace content {
class ContentMainDelegate;
struct ContentMainParams;

extern const char kEmptyTestName[];
extern const char kHelpFlag[];
extern const char kLaunchAsBrowser[];
extern const char kRunManualTestsFlag[];
extern const char kSingleProcessTestsFlag[];

// Flag that causes only the kEmptyTestName test to be run.
extern const char kWarmupFlag[];

// See details in PreRunTest().
class TestState {
 public:
  virtual ~TestState() {}

  // Called once test process has launched (and is still running).
  // NOTE: this is called on a background thread.
  virtual void ChildProcessLaunched(base::ProcessHandle handle,
                                    base::ProcessId pid) = 0;
};

class TestLauncherDelegate {
 public:
  virtual int RunTestSuite(int argc, char** argv) = 0;
  virtual bool AdjustChildProcessCommandLine(
      base::CommandLine* command_line,
      const base::FilePath& temp_data_dir) = 0;
  virtual ContentMainDelegate* CreateContentMainDelegate() = 0;

  // Called prior to running each test. The delegate may alter the CommandLine
  // and options used to launch the subprocess. Additionally the client may
  // return a TestState that is destroyed once the test completes as well as
  // once the test process is launched.
  //
  // NOTE: this is not called if --single_process is supplied.
  virtual std::unique_ptr<TestState> PreRunTest(
      base::CommandLine* command_line,
      base::TestLauncher::LaunchOptions* test_launch_options);

  // Allows a TestLauncherDelegate to adjust the number of |default_jobs| used
  // when --test-launcher-jobs isn't specified on the command-line.
  virtual void AdjustDefaultParallelJobs(int* default_jobs) {}

  // Called prior to returning from LaunchTests(). Gives the delegate a chance
  // to do cleanup before state created by TestLauncher has been destroyed (such
  // as the AtExitManager).
  virtual void OnDoneRunningTests();

 protected:
  virtual ~TestLauncherDelegate();
};

// Launches tests using |launcher_delegate|. |default_jobs| is number
// of test jobs to be run in parallel, unless overridden from the command line.
// Returns exit code.
int LaunchTests(TestLauncherDelegate* launcher_delegate,
                int default_jobs,
                int argc,
                char** argv) WARN_UNUSED_RESULT;

TestLauncherDelegate* GetCurrentTestLauncherDelegate();
ContentMainParams* GetContentMainParams();

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_LAUNCHER_H_
