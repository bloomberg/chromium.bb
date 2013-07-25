// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_launcher.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/app/startup_helper_win.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/test/browser_test.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/base_switches.h"
#include "content/common/sandbox_win.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_types.h"
#elif defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace content {

namespace {

// Tests with this prefix run before the same test without it, and use the same
// profile. i.e. Foo.PRE_Test runs and then Foo.Test. This allows writing tests
// that span browser restarts.
const char kPreTestPrefix[] = "PRE_";

// Manual tests only run when --run-manual is specified. This allows writing
// tests that don't run automatically but are still in the same test binary.
// This is useful so that a team that wants to run a few tests doesn't have to
// add a new binary that must be compiled on all builds.
const char kManualTestPrefix[] = "MANUAL_";

TestLauncherDelegate* g_launcher_delegate;
}

namespace {

int DoRunTestInternal(const testing::TestCase* test_case,
                      const std::string& test_name,
                      const CommandLine& command_line,
                      base::TimeDelta default_timeout,
                      bool* was_timeout) {
  if (test_case) {
    std::string pre_test_name = test_name;
    std::string replace_string = std::string(".") + kPreTestPrefix;
    ReplaceFirstSubstringAfterOffset(&pre_test_name, 0, ".", replace_string);
    for (int i = 0; i < test_case->total_test_count(); ++i) {
      const testing::TestInfo* test_info = test_case->GetTestInfo(i);
      std::string cur_test_name = test_info->test_case_name();
      cur_test_name.append(".");
      cur_test_name.append(test_info->name());
      if (cur_test_name == pre_test_name) {
        int exit_code = DoRunTestInternal(test_case,
                                          pre_test_name,
                                          command_line,
                                          default_timeout,
                                          was_timeout);
        if (exit_code != 0)
          return exit_code;
      }
    }
  }

  CommandLine new_cmd_line(command_line);

  // Always enable disabled tests.  This method is not called with disabled
  // tests unless this flag was specified to the browser test executable.
  new_cmd_line.AppendSwitch("gtest_also_run_disabled_tests");
  new_cmd_line.AppendSwitchASCII("gtest_filter", test_name);
  new_cmd_line.AppendSwitch(kSingleProcessTestsFlag);

  char* browser_wrapper = getenv("BROWSER_WRAPPER");
  int exit_code = base::LaunchChildGTestProcess(
      new_cmd_line,
      browser_wrapper ? browser_wrapper : std::string(),
      default_timeout,
      was_timeout);
  if (*was_timeout) {
    LOG(ERROR) << "Test timeout (" << default_timeout.InMilliseconds()
               << " ms) exceeded for " << test_name;
  }

  return exit_code;
}

// Runs test specified by |test_name| in a child process,
// and returns the exit code.
int DoRunTest(TestLauncherDelegate* launcher_delegate,
              const testing::TestCase* test_case,
              const std::string& test_name,
              base::TimeDelta default_timeout,
              bool* was_timeout) {
  if (was_timeout)
    *was_timeout = false;

#if defined(OS_MACOSX)
  // Some of the below method calls will leak objects if there is no
  // autorelease pool in place.
  base::mac::ScopedNSAutoreleasePool pool;
#endif

  base::ScopedTempDir temp_dir;
  // Create a new data dir and pass it to the child.
  if (!temp_dir.CreateUniqueTempDir() || !temp_dir.IsValid()) {
    LOG(ERROR) << "Error creating temp data directory";
    return -1;
  }

  CommandLine new_cmd_line(*CommandLine::ForCurrentProcess());
  if (!launcher_delegate->AdjustChildProcessCommandLine(&new_cmd_line,
                                                        temp_dir.path())) {
    return -1;
  }

  return DoRunTestInternal(
      test_case, test_name, new_cmd_line, default_timeout, was_timeout);
}

void PrintUsage() {
  fprintf(stdout,
      "Runs tests using the gtest framework, each test being run in its own\n"
      "process.  Any gtest flags can be specified.\n"
      "  --single_process\n"
      "    Runs the tests and the launcher in the same process. Useful for \n"
      "    debugging a specific test in a debugger.\n"
      "  --single-process\n"
      "    Same as above, and also runs Chrome in single-process mode.\n"
      "  --help\n"
      "    Shows this message.\n"
      "  --gtest_help\n"
      "    Shows the gtest help message.\n");
}

// Implementation of base::TestLauncherDelegate. This is also a test launcher,
// wrapping a lower-level test launcher with content-specific code.
class WrapperTestLauncherDelegate : public base::TestLauncherDelegate {
 public:
  explicit WrapperTestLauncherDelegate(
      content::TestLauncherDelegate* launcher_delegate)
      : launcher_delegate_(launcher_delegate),
        timeout_count_(0),
        printed_timeout_message_(false) {
  }

  // base::TestLauncherDelegate:
  virtual bool ShouldRunTest(const testing::TestCase* test_case,
                             const testing::TestInfo* test_info) OVERRIDE;
  virtual void RunTest(
      const testing::TestCase* test_case,
      const testing::TestInfo* test_info,
      const base::TestLauncherDelegate::TestResultCallback& callback) OVERRIDE;
  virtual void RunRemainingTests() OVERRIDE;

 private:
  content::TestLauncherDelegate* launcher_delegate_;

  // Number of times a test timeout occurred.
  size_t timeout_count_;

  // True after a message about too many timeouts has been printed,
  // to avoid doing it more than once.
  bool printed_timeout_message_;

  DISALLOW_COPY_AND_ASSIGN(WrapperTestLauncherDelegate);
};

bool WrapperTestLauncherDelegate::ShouldRunTest(
    const testing::TestCase* test_case,
    const testing::TestInfo* test_info) {
  std::string test_name =
      std::string(test_case->name()) + "." + test_info->name();

  if (StartsWithASCII(test_info->name(), kPreTestPrefix, true))
    return false;

  if (StartsWithASCII(test_info->name(), kManualTestPrefix, true) &&
      !CommandLine::ForCurrentProcess()->HasSwitch(kRunManualTestsFlag)) {
    return false;
  }

  // Stop test execution after too many timeouts.
  if (timeout_count_ > 5) {
    if (!printed_timeout_message_) {
      printed_timeout_message_ = true;
      printf("Too many timeouts, aborting test\n");
    }
    return false;
  }

  return true;
}

void WrapperTestLauncherDelegate::RunTest(
    const testing::TestCase* test_case,
    const testing::TestInfo* test_info,
    const base::TestLauncherDelegate::TestResultCallback& callback) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  bool was_timeout = false;
  std::string test_name =
      std::string(test_case->name()) + "." + test_info->name();
  int exit_code = DoRunTest(launcher_delegate_,
                            test_case,
                            test_name,
                            TestTimeouts::action_max_timeout(),
                            &was_timeout);
  if (was_timeout)
    timeout_count_++;

  base::TestResult result;
  result.test_case_name = test_case->name();
  result.test_name = test_info->name();
  result.success = (exit_code == 0);
  result.elapsed_time = (base::TimeTicks::Now() - start_time);

  callback.Run(result);
}

void WrapperTestLauncherDelegate::RunRemainingTests() {
  // No need to do anything here, we launch tests synchronously.
}

}  // namespace

// The following is kept for historical reasons (so people that are used to
// using it don't get surprised).
const char kChildProcessFlag[]   = "child";

const char kGTestHelpFlag[]   = "gtest_help";

const char kHelpFlag[]   = "help";

const char kLaunchAsBrowser[] = "as-browser";

// See kManualTestPrefix above.
const char kRunManualTestsFlag[] = "run-manual";

const char kSingleProcessTestsFlag[]   = "single_process";


TestLauncherDelegate::~TestLauncherDelegate() {
}

bool ShouldRunContentMain() {
#if defined(OS_WIN) || defined(OS_LINUX)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kProcessType) ||
         command_line->HasSwitch(kLaunchAsBrowser);
#else
  return false;
#endif  // defined(OS_WIN) || defined(OS_LINUX)
}

int RunContentMain(int argc, char** argv,
                   TestLauncherDelegate* launcher_delegate) {
#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  InitializeSandboxInfo(&sandbox_info);
  scoped_ptr<ContentMainDelegate> chrome_main_delegate(
      launcher_delegate->CreateContentMainDelegate());
  return ContentMain(GetModuleHandle(NULL),
                     &sandbox_info,
                     chrome_main_delegate.get());
#elif defined(OS_LINUX)
  scoped_ptr<ContentMainDelegate> chrome_main_delegate(
      launcher_delegate->CreateContentMainDelegate());
  return ContentMain(argc, const_cast<const char**>(argv),
                     chrome_main_delegate.get());
#endif  // defined(OS_WIN)
  NOTREACHED();
  return 0;
}

int LaunchTests(TestLauncherDelegate* launcher_delegate,
                int argc,
                char** argv) {
  DCHECK(!g_launcher_delegate);
  g_launcher_delegate = launcher_delegate;

  CommandLine::Init(argc, argv);
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kHelpFlag)) {
    PrintUsage();
    return 0;
  }

  if (command_line->HasSwitch(kSingleProcessTestsFlag) ||
      (command_line->HasSwitch(switches::kSingleProcess) &&
       command_line->HasSwitch(base::kGTestFilterFlag)) ||
      command_line->HasSwitch(base::kGTestListTestsFlag) ||
      command_line->HasSwitch(kGTestHelpFlag)) {
#if defined(OS_WIN)
    if (command_line->HasSwitch(kSingleProcessTestsFlag)) {
      sandbox::SandboxInterfaceInfo sandbox_info;
      InitializeSandboxInfo(&sandbox_info);
      InitializeSandbox(&sandbox_info);
    }
#endif
    return launcher_delegate->RunTestSuite(argc, argv);
  }

  if (ShouldRunContentMain())
    return RunContentMain(argc, argv, launcher_delegate);

  fprintf(stdout,
      "Starting tests...\n"
      "IMPORTANT DEBUGGING NOTE: each test is run inside its own process.\n"
      "For debugging a test inside a debugger, use the\n"
      "--gtest_filter=<your_test_name> flag along with either\n"
      "--single_process (to run the test in one launcher/browser process) or\n"
      "--single-process (to do the above, and also run Chrome in single-"
      "process mode).\n");

  base::AtExitManager at_exit;
  testing::InitGoogleTest(&argc, argv);
  TestTimeouts::Initialize();

  WrapperTestLauncherDelegate delegate(launcher_delegate);
  return base::LaunchTests(&delegate, argc, argv);
}

TestLauncherDelegate* GetCurrentTestLauncherDelegate() {
  return g_launcher_delegate;
}

}  // namespace content
