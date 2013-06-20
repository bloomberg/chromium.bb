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
#include "base/process_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
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
                      CommandLine* command_line,
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

  CommandLine new_cmd_line(*command_line);

  // Always enable disabled tests.  This method is not called with disabled
  // tests unless this flag was specified to the browser test executable.
  new_cmd_line.AppendSwitch("gtest_also_run_disabled_tests");
  new_cmd_line.AppendSwitchASCII("gtest_filter", test_name);
  new_cmd_line.AppendSwitch(kSingleProcessTestsFlag);

  const char* browser_wrapper = getenv("BROWSER_WRAPPER");
  if (browser_wrapper) {
#if defined(OS_WIN)
    new_cmd_line.PrependWrapper(ASCIIToWide(browser_wrapper));
#elif defined(OS_POSIX)
    new_cmd_line.PrependWrapper(browser_wrapper);
#endif
    VLOG(1) << "BROWSER_WRAPPER was set, prefixing command_line with "
            << browser_wrapper;
  }

  base::ProcessHandle process_handle;
  base::LaunchOptions options;

#if defined(OS_POSIX)
  // On POSIX, we launch the test in a new process group with pgid equal to
  // its pid. Any child processes that the test may create will inherit the
  // same pgid. This way, if the test is abruptly terminated, we can clean up
  // any orphaned child processes it may have left behind.
  options.new_process_group = true;
#endif

  if (!base::LaunchProcess(new_cmd_line, options, &process_handle))
    return -1;

  int exit_code = 0;
  if (!base::WaitForExitCodeWithTimeout(process_handle,
                                        &exit_code,
                                        default_timeout)) {
    LOG(ERROR) << "Test timeout (" << default_timeout.InMilliseconds()
               << " ms) exceeded for " << test_name;

    if (was_timeout)
      *was_timeout = true;
    exit_code = -1;  // Set a non-zero exit code to signal a failure.

    // Ensure that the process terminates.
    base::KillProcess(process_handle, -1, true);
  }

#if defined(OS_POSIX)
  if (exit_code != 0) {
    // On POSIX, in case the test does not exit cleanly, either due to a crash
    // or due to it timing out, we need to clean up any child processes that
    // it might have created. On Windows, child processes are automatically
    // cleaned up using JobObjects.
    base::KillProcessGroup(process_handle);
  }
#endif

  base::CloseProcessHandle(process_handle);

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

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  CommandLine new_cmd_line(cmd_line->GetProgram());
  CommandLine::SwitchMap switches = cmd_line->GetSwitches();

  // Strip out gtest_output flag because otherwise we would overwrite results
  // of the previous test. We will generate the final output file later
  // in RunTests().
  switches.erase(base::kGTestOutputFlag);

  // Strip out gtest_repeat flag because we can only run one test in the child
  // process (restarting the browser in the same process is illegal after it
  // has been shut down and will actually crash).
  switches.erase(base::kGTestRepeatFlag);

  for (CommandLine::SwitchMap::const_iterator iter = switches.begin();
       iter != switches.end(); ++iter) {
    new_cmd_line.AppendSwitchNative((*iter).first, (*iter).second);
  }

  base::ScopedTempDir temp_dir;
  // Create a new data dir and pass it to the child.
  if (!temp_dir.CreateUniqueTempDir() || !temp_dir.IsValid()) {
    LOG(ERROR) << "Error creating temp data directory";
    return -1;
  }

  if (!launcher_delegate->AdjustChildProcessCommandLine(&new_cmd_line,
                                                        temp_dir.path())) {
    return -1;
  }

  return DoRunTestInternal(
      test_case, test_name, &new_cmd_line, default_timeout, was_timeout);
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
  virtual bool RunTest(const testing::TestCase* test_case,
                       const testing::TestInfo* test_info) OVERRIDE;

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

bool WrapperTestLauncherDelegate::RunTest(const testing::TestCase* test_case,
                                          const testing::TestInfo* test_info) {
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
  return exit_code == 0;
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
