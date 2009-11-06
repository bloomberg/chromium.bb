// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"

#include "chrome/test/test_launcher/test_runner.h"
#include "chrome/test/unit/chrome_test_suite.h"

// This version of the test launcher forks a new process for each test it runs.

namespace {

const char kGTestListTestsFlag[] = "gtest_list_tests";
const char kChildProcessFlag[]   = "child";

class OutOfProcTestRunner : public tests::TestRunner {
 public:
  OutOfProcTestRunner() {
  }

  virtual ~OutOfProcTestRunner() {
  }

  bool Init() {
    return true;
  }

  // Returns true if the test succeeded, false if it failed.
  bool RunTest(const std::string& test_name) {
    const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
#if defined(OS_WIN)
    CommandLine new_cmd_line =
        CommandLine::FromString(cmd_line->command_line_string());
#else
    CommandLine new_cmd_line(cmd_line->argv());
#endif
    // Always enable disabled tests.  This method is not called with disabled
    // tests unless this flag was specified to the browser test executable.
    new_cmd_line.AppendSwitch("gtest_also_run_disabled_tests");
    new_cmd_line.AppendSwitchWithValue("gtest_filter", test_name);
    new_cmd_line.AppendSwitch(kChildProcessFlag);

    base::ProcessHandle process_handle;
    bool r = base::LaunchApp(new_cmd_line, false, false, &process_handle);
    if (!r)
      return false;

    int exit_code = 0;
    r = base::WaitForExitCode(process_handle, &exit_code);
    if (!r)
      return false;

    return exit_code == 0;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OutOfProcTestRunner);
};

class OutOfProcTestRunnerFactory : public tests::TestRunnerFactory {
 public:
  OutOfProcTestRunnerFactory() { }

  virtual tests::TestRunner* CreateTestRunner() const {
    return new OutOfProcTestRunner();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OutOfProcTestRunnerFactory);
};

}  // namespace

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kChildProcessFlag))
    return ChromeTestSuite(argc, argv).Run();

  if (command_line->HasSwitch(kGTestListTestsFlag))
    return ChromeTestSuite(argc, argv).Run();

  OutOfProcTestRunnerFactory test_runner_factory;
  return tests::RunTests(test_runner_factory) ? 0 : 1;
}
