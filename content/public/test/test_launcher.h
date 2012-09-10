// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_LAUNCHER_H_
#define CONTENT_PUBLIC_TEST_TEST_LAUNCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"

class CommandLine;
class FilePath;

namespace base {
class RunLoop;
}

namespace content {
class ContentMainDelegate;
}

namespace test_launcher {

extern const char kEmptyTestName[];
extern const char kGTestFilterFlag[];
extern const char kGTestHelpFlag[];
extern const char kGTestListTestsFlag[];
extern const char kGTestRepeatFlag[];
extern const char kGTestRunDisabledTestsFlag[];
extern const char kGTestOutputFlag[];
extern const char kLaunchAsBrowser[];
extern const char kSingleProcessTestsFlag[];
extern const char kSingleProcessTestsAndChromeFlag[];
extern const char kRunManualTestsFlag[];
extern const char kHelpFlag[];

// Flag that causes only the kEmptyTestName test to be run.
extern const char kWarmupFlag[];

class TestLauncherDelegate {
 public:
  virtual std::string GetEmptyTestName() = 0;
  virtual int RunTestSuite(int argc, char** argv) = 0;
  virtual bool AdjustChildProcessCommandLine(CommandLine* command_line,
                                             const FilePath& temp_data_dir) = 0;
  virtual void PreRunMessageLoop(base::RunLoop* run_loop) {}
  virtual void PostRunMessageLoop() {}
  virtual content::ContentMainDelegate* CreateContentMainDelegate() = 0;

 protected:
  virtual ~TestLauncherDelegate();
};

int LaunchTests(TestLauncherDelegate* launcher_delegate,
                int argc,
                char** argv) WARN_UNUSED_RESULT;

TestLauncherDelegate* GetCurrentTestLauncherDelegate();

}  // namespace test_launcher

#endif  // CONTENT_PUBLIC_TEST_TEST_LAUNCHER_H_
