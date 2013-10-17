// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_launcher.h"

#include <map>
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
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/launcher/parallel_test_launcher.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
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

std::string RemoveAnyPrePrefixes(const std::string& test_name) {
  std::string result(test_name);
  ReplaceSubstringsAfterOffset(&result, 0, kPreTestPrefix, std::string());
  return result;
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
  WrapperTestLauncherDelegate(content::TestLauncherDelegate* launcher_delegate,
                              size_t jobs)
      : launcher_delegate_(launcher_delegate),
        timeout_count_(0),
        printed_timeout_message_(false),
        parallel_launcher_(jobs) {
    CHECK(temp_dir_.CreateUniqueTempDir());
  }

  // base::TestLauncherDelegate:
  virtual void OnTestIterationStarting() OVERRIDE;
  virtual std::string GetTestNameForFiltering(
      const testing::TestCase* test_case,
      const testing::TestInfo* test_info) OVERRIDE;
  virtual bool ShouldRunTest(const testing::TestCase* test_case,
                             const testing::TestInfo* test_info) OVERRIDE;
  virtual void RunTest(
      const testing::TestCase* test_case,
      const testing::TestInfo* test_info,
      const base::TestLauncherDelegate::TestResultCallback& callback) OVERRIDE;
  virtual void RunRemainingTests() OVERRIDE;

 private:
  struct TestInfo {
    std::string GetFullName() const { return test_case_name + "." + test_name; }

    std::string test_case_name;
    std::string test_name;
    std::vector<base::TestLauncherDelegate::TestResultCallback> callbacks;
  };

  // Launches test from |test_info| using |command_line| and parallel launcher.
  void DoRunTest(const TestInfo& test_info, const CommandLine& command_line);

  // Launches test named |test_name| using |command_line| and parallel launcher,
  // given result of PRE_ test |pre_test_result|.
  void RunDependentTest(const std::string test_name,
                        const CommandLine& command_line,
                        const base::TestResult& pre_test_result);

  // Callback to receive result of a test.
  void GTestCallback(
      const TestInfo& test_info,
      int exit_code,
      const base::TimeDelta& elapsed_time,
      bool was_timeout,
      const std::string& output);

  content::TestLauncherDelegate* launcher_delegate_;

  // Number of times a test timeout occurred.
  size_t timeout_count_;

  // True after a message about too many timeouts has been printed,
  // to avoid doing it more than once.
  bool printed_timeout_message_;

  base::ParallelTestLauncher parallel_launcher_;

  // Store all tests to run before running any of them to properly
  // handle PRE_ tests. The map is indexed by test full name (e.g. "A.B").
  typedef std::map<std::string, TestInfo> TestInfoMap;
  TestInfoMap tests_to_run_;

  // Temporary directory for user data directories.
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(WrapperTestLauncherDelegate);
};

void WrapperTestLauncherDelegate::OnTestIterationStarting() {
  tests_to_run_.clear();
}

std::string WrapperTestLauncherDelegate::GetTestNameForFiltering(
    const testing::TestCase* test_case,
    const testing::TestInfo* test_info) {
  return RemoveAnyPrePrefixes(
      std::string(test_case->name()) + "." + test_info->name());
}

bool WrapperTestLauncherDelegate::ShouldRunTest(
    const testing::TestCase* test_case,
    const testing::TestInfo* test_info) {
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
  TestInfo run_test_info;
  run_test_info.test_case_name = test_case->name();
  run_test_info.test_name = test_info->name();
  run_test_info.callbacks.push_back(callback);

  DCHECK(!ContainsKey(tests_to_run_, run_test_info.GetFullName()));
  tests_to_run_[run_test_info.GetFullName()] = run_test_info;
}

void WrapperTestLauncherDelegate::RunRemainingTests() {
  // PRE_ tests and tests that depend on them must share the same
  // data directory. Using test name as directory name leads to too long
  // names (exceeding UNIX_PATH_MAX, which creates a problem with
  // process_singleton_linux). Create a randomly-named temporary directory
  // and keep track of the names so that PRE_ tests can still re-use them.
  std::map<std::string, base::FilePath> temp_directories;

  // List of tests we can kick off right now, depending on no other tests.
  std::vector<std::pair<std::string, CommandLine> > tests_to_run_now;

  for (TestInfoMap::iterator i = tests_to_run_.begin();
       i != tests_to_run_.end();
       ++i) {
    const TestInfo& test_info = i->second;

    // Make sure PRE_ tests and tests that depend on them share the same
    // data directory - based it on the test name without prefixes.
    std::string test_name_no_pre(RemoveAnyPrePrefixes(test_info.GetFullName()));
    if (!ContainsKey(temp_directories, test_name_no_pre)) {
      base::FilePath temp_dir;
      CHECK(file_util::CreateTemporaryDirInDir(
                temp_dir_.path(), FILE_PATH_LITERAL("d"), &temp_dir));
      temp_directories[test_name_no_pre] = temp_dir;
    }

    CommandLine new_cmd_line(*CommandLine::ForCurrentProcess());
    CHECK(launcher_delegate_->AdjustChildProcessCommandLine(
              &new_cmd_line, temp_directories[test_name_no_pre]));

    std::string pre_test_name(
        test_info.test_case_name + "." + kPreTestPrefix + test_info.test_name);
    if (ContainsKey(tests_to_run_, pre_test_name)) {
      tests_to_run_[pre_test_name].callbacks.push_back(
          base::Bind(&WrapperTestLauncherDelegate::RunDependentTest,
                     base::Unretained(this),
                     test_info.GetFullName(),
                     new_cmd_line));
    } else {
      tests_to_run_now.push_back(
          std::make_pair(test_info.GetFullName(), new_cmd_line));
    }
  }

  for (size_t i = 0; i < tests_to_run_now.size(); i++) {
    const TestInfo& test_info = tests_to_run_[tests_to_run_now[i].first];
    const CommandLine& cmd_line = tests_to_run_now[i].second;
    DoRunTest(test_info, cmd_line);
  }
}

void WrapperTestLauncherDelegate::DoRunTest(const TestInfo& test_info,
                                            const CommandLine& command_line) {
  CommandLine new_cmd_line(command_line.GetProgram());
  CommandLine::SwitchMap switches = command_line.GetSwitches();

  // Strip out gtest_output flag because otherwise we would overwrite results
  // of the other tests.
  switches.erase(base::kGTestOutputFlag);

  for (CommandLine::SwitchMap::const_iterator iter = switches.begin();
       iter != switches.end(); ++iter) {
    new_cmd_line.AppendSwitchNative(iter->first, iter->second);
  }

  // Always enable disabled tests.  This method is not called with disabled
  // tests unless this flag was specified to the browser test executable.
  new_cmd_line.AppendSwitch("gtest_also_run_disabled_tests");
  new_cmd_line.AppendSwitchASCII(
      "gtest_filter",
      test_info.test_case_name + "." + test_info.test_name);
  new_cmd_line.AppendSwitch(kSingleProcessTestsFlag);

  char* browser_wrapper = getenv("BROWSER_WRAPPER");

  // PRE_ tests and tests that depend on them should share the sequence token
  // name, so that they are run serially.
  std::string test_name_no_pre = RemoveAnyPrePrefixes(
      test_info.test_case_name + "." + test_info.test_name);

  parallel_launcher_.LaunchChildGTestProcess(
      new_cmd_line,
      browser_wrapper ? browser_wrapper : std::string(),
      TestTimeouts::action_max_timeout(),
      base::Bind(&WrapperTestLauncherDelegate::GTestCallback,
                 base::Unretained(this),
                 test_info));
}

void WrapperTestLauncherDelegate::RunDependentTest(
    const std::string test_name,
    const CommandLine& command_line,
    const base::TestResult& pre_test_result) {
  const TestInfo& test_info = tests_to_run_[test_name];
  if (pre_test_result.status == base::TestResult::TEST_SUCCESS) {
    // Only run the dependent test if PRE_ test succeeded.
    DoRunTest(test_info, command_line);
  } else {
    // Otherwise skip the test.
    base::TestResult test_result;
    test_result.test_case_name = test_info.test_case_name;
    test_result.test_name = test_info.test_name;
    test_result.status = base::TestResult::TEST_SKIPPED;
    for (size_t i = 0; i < test_info.callbacks.size(); i++)
      test_info.callbacks[i].Run(test_result);
  }
}

void WrapperTestLauncherDelegate::GTestCallback(
    const TestInfo& test_info,
    int exit_code,
    const base::TimeDelta& elapsed_time,
    bool was_timeout,
    const std::string& output) {
  base::TestResult result;
  result.test_case_name = test_info.test_case_name;
  result.test_name = test_info.test_name;

  // TODO(phajdan.jr): Recognize crashes.
  if (exit_code == 0)
    result.status = base::TestResult::TEST_SUCCESS;
  else if (was_timeout)
    result.status = base::TestResult::TEST_TIMEOUT;
  else
    result.status = base::TestResult::TEST_FAILURE;

  result.elapsed_time = elapsed_time;

  // TODO(phajdan.jr): Use base::PrintTestOutputSnippetOnFailure after migrating
  // away from run_test_cases.py (http://crbug.com/236893).
  fprintf(stdout, "%s", output.c_str());
  fflush(stdout);

  for (size_t i = 0; i < test_info.callbacks.size(); i++)
    test_info.callbacks[i].Run(result);
  parallel_launcher_.ResetOutputWatchdog();
}

bool GetSwitchValueAsInt(const std::string& switch_name, int* result) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switch_name))
    return true;

  std::string switch_value =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switch_name);
  if (!base::StringToInt(switch_value, result) || *result < 1) {
    LOG(ERROR) << "Invalid value for " << switch_name << ": " << switch_value;
    return false;
  }

  return true;
}

}  // namespace

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
      command_line->HasSwitch(base::kGTestHelpFlag)) {
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

  int jobs = 1;  // TODO(phajdan.jr): Default to half the number of CPU cores.
  if (!GetSwitchValueAsInt(switches::kTestLauncherJobs, &jobs))
    return 1;

  base::MessageLoopForIO message_loop;

  WrapperTestLauncherDelegate delegate(launcher_delegate, jobs);
  base::TestLauncher launcher(&delegate);
  bool success = launcher.Run(argc, argv);
  return (success ? 0 : 1);
}

TestLauncherDelegate* GetCurrentTestLauncherDelegate() {
  return g_launcher_delegate;
}

}  // namespace content
