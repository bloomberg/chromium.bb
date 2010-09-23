// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "base/test/test_suite.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/test_launcher/test_runner.h"
#include "chrome/test/unit/chrome_test_suite.h"

#if defined(OS_WIN)
#include "base/base_switches.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/sandbox_policy.h"
#include "sandbox/src/dep.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/sandbox_types.h"

// The entry point signature of chrome.dll.
typedef int (*DLL_MAIN)(HINSTANCE, sandbox::SandboxInterfaceInfo*, wchar_t*);
#endif

// This version of the test launcher forks a new process for each test it runs.

namespace {

const char kGTestHelpFlag[]   = "gtest_help";
const char kGTestListTestsFlag[] = "gtest_list_tests";
const char kGTestOutputFlag[] = "gtest_output";
const char kGTestRepeatFlag[] = "gtest_repeat";
const char kSingleProcessTestsFlag[]   = "single_process";
const char kSingleProcessTestsAndChromeFlag[]   = "single-process";
const char kTestTerminateTimeoutFlag[] = "test-terminate-timeout";
// The following is kept for historical reasons (so people that are used to
// using it don't get surprised).
const char kChildProcessFlag[]   = "child";
const char kHelpFlag[]   = "help";

// This value was changed from 30000 (30sec) to 45000 due to
// http://crbug.com/43862.
const int64 kDefaultTestTimeoutMs = 45000;

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
    // Some of the below method calls will leak objects if there is no
    // autorelease pool in place.
    base::ScopedNSAutoreleasePool pool;

    const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
    CommandLine new_cmd_line(cmd_line->GetProgram());
    CommandLine::SwitchMap switches = cmd_line->GetSwitches();

    // Strip out gtest_output flag because otherwise we would overwrite results
    // of the previous test. We will generate the final output file later
    // in RunTests().
    switches.erase(kGTestOutputFlag);

    // Strip out gtest_repeat flag because we can only run one test in the child
    // process (restarting the browser in the same process is illegal after it
    // has been shut down and will actually crash).
    switches.erase(kGTestRepeatFlag);

    // Strip out user-data-dir if present.  We will add it back in again later.
    switches.erase(switches::kUserDataDir);

    for (CommandLine::SwitchMap::const_iterator iter = switches.begin();
         iter != switches.end(); ++iter) {
      new_cmd_line.AppendSwitchNative((*iter).first, (*iter).second);
    }

    // Always enable disabled tests.  This method is not called with disabled
    // tests unless this flag was specified to the browser test executable.
    new_cmd_line.AppendSwitch("gtest_also_run_disabled_tests");
    new_cmd_line.AppendSwitchASCII("gtest_filter", test_name);
    new_cmd_line.AppendSwitch(kChildProcessFlag);

    // Do not let the child ignore failures.  We need to propagate the
    // failure status back to the parent.
    new_cmd_line.AppendSwitch(base::TestSuite::kStrictFailureHandling);

    // Create a new user data dir and pass it to the child.
    ScopedTempDir temp_dir;
    if (!temp_dir.CreateUniqueTempDir() || !temp_dir.IsValid()) {
      LOG(ERROR) << "Error creating temp profile directory";
      return false;
    }
    new_cmd_line.AppendSwitchPath(switches::kUserDataDir, temp_dir.path());

    base::ProcessHandle process_handle;
#if defined(OS_POSIX)
    // On POSIX, we launch the test in a new process group with pgid equal to
    // its pid. Any child processes that the test may create will inherit the
    // same pgid. This way, if the test is abruptly terminated, we can clean up
    // any orphaned child processes it may have left behind.
    base::environment_vector no_env;
    base::file_handle_mapping_vector no_files;
    if (!base::LaunchAppInNewProcessGroup(new_cmd_line.argv(), no_env, no_files,
                                          false, &process_handle))
#else
    if (!base::LaunchApp(new_cmd_line, false, false, &process_handle))
#endif
      return false;

    int test_terminate_timeout_ms = kDefaultTestTimeoutMs;
    if (cmd_line->HasSwitch(kTestTerminateTimeoutFlag)) {
      std::string timeout_str =
          cmd_line->GetSwitchValueASCII(kTestTerminateTimeoutFlag);
      int timeout;
      base::StringToInt(timeout_str, &timeout);
      test_terminate_timeout_ms = std::max(test_terminate_timeout_ms, timeout);
    }

    int exit_code = 0;
    if (!base::WaitForExitCodeWithTimeout(process_handle, &exit_code,
                                          test_terminate_timeout_ms)) {
      LOG(ERROR) << "Test timeout (" << test_terminate_timeout_ms
                 << " ms) exceeded for " << test_name;

      exit_code = -1;  // Set a non-zero exit code to signal a failure.

      // Ensure that the process terminates.
      base::KillProcess(process_handle, -1, true);

#if defined(OS_POSIX)
      // On POSIX, we need to clean up any child processes that the test might
      // have created. On windows, child processes are automatically cleaned up
      // using JobObjects.
      base::KillProcessGroup(process_handle);
#endif
    }

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

void PrintUsage() {
  fprintf(stdout,
      "Runs tests using the gtest framework, each test being run in its own\n"
      "process.  Any gtest flags can be specified.\n"
      "  --single_process\n"
      "    Runs the tests and the launcher in the same process. Useful for \n"
      "    debugging a specific test in a debugger.\n"
      "  --single-process\n"
      "    Same as above, and also runs Chrome in single-process mode.\n"
      "  --test-terminate-timeout\n"
      "    Specifies a timeout (in milliseconds) after which a running test\n"
      "    will be forcefully terminated.\n"
      "  --help\n"
      "    Shows this message.\n"
      "  --gtest_help\n"
      "    Shows the gtest help message.\n");
}

}  // namespace

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kHelpFlag)) {
    PrintUsage();
    return 0;
  }

  // TODO(pkasting): This "single_process vs. single-process" design is terrible
  // UI.  Instead, there should be some sort of signal flag on the command line,
  // with all subsequent arguments passed through to the underlying browser.
  if (command_line->HasSwitch(kChildProcessFlag) ||
      command_line->HasSwitch(kSingleProcessTestsFlag) ||
      command_line->HasSwitch(kSingleProcessTestsAndChromeFlag) ||
      command_line->HasSwitch(kGTestListTestsFlag) ||
      command_line->HasSwitch(kGTestHelpFlag)) {

#if defined(OS_WIN)
    if (command_line->HasSwitch(kChildProcessFlag) ||
        command_line->HasSwitch(kSingleProcessTestsFlag)) {
      // This is the browser process, so setup the sandbox broker.
      sandbox::BrokerServices* broker_services =
          sandbox::SandboxFactory::GetBrokerServices();
      if (broker_services) {
        sandbox::InitBrokerServices(broker_services);
        // Precreate the desktop and window station used by the renderers.
        sandbox::TargetPolicy* policy = broker_services->CreatePolicy();
        sandbox::ResultCode result = policy->CreateAlternateDesktop(true);
        CHECK(sandbox::SBOX_ERROR_FAILED_TO_SWITCH_BACK_WINSTATION != result);
        policy->Release();
      }
    }
#endif
    return ChromeTestSuite(argc, argv).Run();
  }

#if defined(OS_WIN)
  if (command_line->HasSwitch(switches::kProcessType)) {
    // This is a child process, call ChromeMain.
    FilePath chrome_path(command_line->GetProgram().DirName());
    chrome_path = chrome_path.Append(chrome::kBrowserResourcesDll);
    HMODULE dll = LoadLibrary(chrome_path.value().c_str());
    DLL_MAIN entry_point =
        reinterpret_cast<DLL_MAIN>(::GetProcAddress(dll, "ChromeMain"));
    if (!entry_point)
      return -1;

    // Initialize the sandbox services.
    sandbox::SandboxInterfaceInfo sandbox_info = {0};
    sandbox_info.target_services = sandbox::SandboxFactory::GetTargetServices();
    return entry_point(GetModuleHandle(NULL), &sandbox_info, GetCommandLineW());
  }
#endif

  fprintf(stdout,
      "Starting tests...\n"
      "IMPORTANT DEBUGGING NOTE: each test is run inside its own process.\n"
      "For debugging a test inside a debugger, use the\n"
      "--gtest_filter=<your_test_name> flag along with either\n"
      "--single_process (to run all tests in one launcher/browser process) or\n"
      "--single-process (to do the above, and also run Chrome in single-\n"
      "process mode).\n");
  OutOfProcTestRunnerFactory test_runner_factory;

  int cycles = 1;
  if (command_line->HasSwitch(kGTestRepeatFlag)) {
    base::StringToInt(command_line->GetSwitchValueASCII(kGTestRepeatFlag),
                      &cycles);
  }

  int exit_code = 0;
  while (cycles != 0) {
    if (!tests::RunTests(test_runner_factory)) {
      exit_code = 1;
      break;
    }

    // Special value "-1" means "repeat indefinitely".
    if (cycles != -1)
      cycles--;
  }
  return exit_code;
}
