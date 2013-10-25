// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_LAUNCHER_H_
#define CONTENT_PUBLIC_TEST_TEST_LAUNCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"

class CommandLine;

namespace base {
class FilePath;
class RunLoop;
}

namespace content {
class ContentMainDelegate;

extern const char kEmptyTestName[];
extern const char kHelpFlag[];
extern const char kLaunchAsBrowser[];
extern const char kRunManualTestsFlag[];
extern const char kSingleProcessTestsFlag[];

// Flag that causes only the kEmptyTestName test to be run.
extern const char kWarmupFlag[];

class TestLauncherDelegate {
 public:
  virtual int RunTestSuite(int argc, char** argv) = 0;
  virtual bool AdjustChildProcessCommandLine(
      CommandLine* command_line,
      const base::FilePath& temp_data_dir) = 0;
  virtual void PreRunMessageLoop(base::RunLoop* run_loop) {}
  virtual void PostRunMessageLoop() {}
  virtual ContentMainDelegate* CreateContentMainDelegate() = 0;

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

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_LAUNCHER_H_
