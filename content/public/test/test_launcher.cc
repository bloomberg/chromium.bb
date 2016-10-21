// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_launcher.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/test/browser_test.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include "base/files/file_descriptor_watcher_posix.h"
#endif

#if defined(OS_WIN)
#include "base/base_switches.h"
#include "content/common/sandbox_win.h"
#include "content/public/app/sandbox_helper_win.h"
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
ContentMainParams* g_params;

std::string RemoveAnyPrePrefixes(const std::string& test_name) {
  std::string result(test_name);
  base::ReplaceSubstringsAfterOffset(
      &result, 0, kPreTestPrefix, base::StringPiece());
  return result;
}

void PrintUsage() {
  fprintf(stdout,
          "Runs tests using the gtest framework, each batch of tests being\n"
          "run in their own process. Supported command-line flags:\n"
          "\n"
          " Common flags:\n"
          "  --gtest_filter=...\n"
          "    Runs a subset of tests (see --gtest_help for more info).\n"
          "\n"
          "  --help\n"
          "    Shows this message.\n"
          "\n"
          "  --gtest_help\n"
          "    Shows the gtest help message.\n"
          "\n"
          "  --test-launcher-jobs=N\n"
          "    Sets the number of parallel test jobs to N.\n"
          "\n"
          "  --single_process\n"
          "    Runs the tests and the launcher in the same process. Useful\n"
          "    for debugging a specific test in a debugger.\n"
          "\n"
          " Other flags:\n"
          "  --test-launcher-retry-limit=N\n"
          "    Sets the limit of test retries on failures to N.\n"
          "\n"
          "  --test-launcher-summary-output=PATH\n"
          "    Saves a JSON machine-readable summary of the run.\n"
          "\n"
          "  --test-launcher-print-test-stdio=auto|always|never\n"
          "    Controls when full test output is printed.\n"
          "    auto means to print it when the test failed.\n"
          "\n"
          "  --test-launcher-total-shards=N\n"
          "    Sets the total number of shards to N.\n"
          "\n"
          "  --test-launcher-shard-index=N\n"
          "    Sets the shard index to run to N (from 0 to TOTAL - 1).\n");
}

void CallChildProcessLaunched(TestState* test_state,
                              base::ProcessHandle handle,
                              base::ProcessId pid) {
  if (test_state)
    test_state->ChildProcessLaunched(handle, pid);
}

// Implementation of base::TestLauncherDelegate. This is also a test launcher,
// wrapping a lower-level test launcher with content-specific code.
class WrapperTestLauncherDelegate : public base::TestLauncherDelegate {
 public:
  explicit WrapperTestLauncherDelegate(
      content::TestLauncherDelegate* launcher_delegate)
      : launcher_delegate_(launcher_delegate) {
    CHECK(temp_dir_.CreateUniqueTempDir());
  }

  // base::TestLauncherDelegate:
  bool GetTests(std::vector<base::TestIdentifier>* output) override;
  bool ShouldRunTest(const std::string& test_case_name,
                     const std::string& test_name) override;
  size_t RunTests(base::TestLauncher* test_launcher,
                  const std::vector<std::string>& test_names) override;
  size_t RetryTests(base::TestLauncher* test_launcher,
                    const std::vector<std::string>& test_names) override;

 private:
  void DoRunTests(base::TestLauncher* test_launcher,
                  const std::vector<std::string>& test_names);

  // Launches test named |test_name| using parallel launcher,
  // given result of PRE_ test |pre_test_result|.
  void RunDependentTest(base::TestLauncher* test_launcher,
                        const std::string test_name,
                        const base::TestResult& pre_test_result);

  // Callback to receive result of a test.
  void GTestCallback(base::TestLauncher* test_launcher,
                     const std::vector<std::string>& test_names,
                     const std::string& test_name,
                     std::unique_ptr<TestState> test_state,
                     int exit_code,
                     const base::TimeDelta& elapsed_time,
                     bool was_timeout,
                     const std::string& output);

  content::TestLauncherDelegate* launcher_delegate_;

  // Store dependent test name (map is indexed by full test name).
  typedef std::map<std::string, std::string> DependentTestMap;
  DependentTestMap dependent_test_map_;
  DependentTestMap reverse_dependent_test_map_;

  // Store unique data directory prefix for test names (without PRE_ prefixes).
  // PRE_ tests and tests that depend on them must share the same
  // data directory. Using test name as directory name leads to too long
  // names (exceeding UNIX_PATH_MAX, which creates a problem with
  // process_singleton_linux). Create a randomly-named temporary directory
  // and keep track of the names so that PRE_ tests can still re-use them.
  typedef std::map<std::string, base::FilePath> UserDataDirMap;
  UserDataDirMap user_data_dir_map_;

  // Store names of all seen tests to properly handle PRE_ tests.
  std::set<std::string> all_test_names_;

  // Temporary directory for user data directories.
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(WrapperTestLauncherDelegate);
};

bool WrapperTestLauncherDelegate::GetTests(
    std::vector<base::TestIdentifier>* output) {
  *output = base::GetCompiledInTests();
  return true;
}

bool WrapperTestLauncherDelegate::ShouldRunTest(
    const std::string& test_case_name,
    const std::string& test_name) {
  all_test_names_.insert(test_case_name + "." + test_name);

  if (base::StartsWith(test_name, kManualTestPrefix,
                       base::CompareCase::SENSITIVE) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(kRunManualTestsFlag)) {
    return false;
  }

  if (base::StartsWith(test_name, kPreTestPrefix,
                       base::CompareCase::SENSITIVE)) {
    // We will actually run PRE_ tests, but to ensure they run on the same shard
    // as dependent tests, handle all these details internally.
    return false;
  }

  return true;
}

std::string GetPreTestName(const std::string& full_name) {
  size_t dot_pos = full_name.find('.');
  CHECK_NE(dot_pos, std::string::npos);
  std::string test_case_name = full_name.substr(0, dot_pos);
  std::string test_name = full_name.substr(dot_pos + 1);
  return test_case_name + "." + kPreTestPrefix + test_name;
}

size_t WrapperTestLauncherDelegate::RunTests(
    base::TestLauncher* test_launcher,
    const std::vector<std::string>& test_names) {
  dependent_test_map_.clear();
  reverse_dependent_test_map_.clear();
  user_data_dir_map_.clear();

  // Number of additional tests to run because of dependencies.
  size_t additional_tests_to_run_count = 0;

  // Compute dependencies of tests to be run.
  for (size_t i = 0; i < test_names.size(); i++) {
    std::string full_name(test_names[i]);
    std::string pre_test_name(GetPreTestName(full_name));

    while (base::ContainsKey(all_test_names_, pre_test_name)) {
      additional_tests_to_run_count++;

      DCHECK(!base::ContainsKey(dependent_test_map_, pre_test_name));
      dependent_test_map_[pre_test_name] = full_name;

      DCHECK(!base::ContainsKey(reverse_dependent_test_map_, full_name));
      reverse_dependent_test_map_[full_name] = pre_test_name;

      full_name = pre_test_name;
      pre_test_name = GetPreTestName(pre_test_name);
    }
  }

  for (size_t i = 0; i < test_names.size(); i++) {
    std::string full_name(test_names[i]);

    // Make sure no PRE_ tests were requested explicitly.
    DCHECK_EQ(full_name, RemoveAnyPrePrefixes(full_name));

    if (!base::ContainsKey(user_data_dir_map_, full_name)) {
      base::FilePath temp_dir;
      CHECK(base::CreateTemporaryDirInDir(temp_dir_.GetPath(),
                                          FILE_PATH_LITERAL("d"), &temp_dir));
      user_data_dir_map_[full_name] = temp_dir;
    }

    // If the test has any dependencies, get to the root and start with that.
    while (base::ContainsKey(reverse_dependent_test_map_, full_name))
      full_name = GetPreTestName(full_name);

    std::vector<std::string> test_list;
    test_list.push_back(full_name);
    DoRunTests(test_launcher, test_list);
  }

  return test_names.size() + additional_tests_to_run_count;
}

size_t WrapperTestLauncherDelegate::RetryTests(
    base::TestLauncher* test_launcher,
    const std::vector<std::string>& test_names) {
  // List of tests we can kick off right now, depending on no other tests.
  std::vector<std::string> tests_to_run_now;

  // We retry at least the tests requested to retry.
  std::set<std::string> test_names_set(test_names.begin(), test_names.end());

  // In the face of PRE_ tests, we need to retry the entire chain of tests,
  // from the very first one.
  for (size_t i = 0; i < test_names.size(); i++) {
    std::string test_name(test_names[i]);
    while (base::ContainsKey(reverse_dependent_test_map_, test_name)) {
      test_name = reverse_dependent_test_map_[test_name];
      test_names_set.insert(test_name);
    }
  }

  // Discard user data directories from any previous runs. Start with
  // fresh state.
  for (UserDataDirMap::const_iterator i = user_data_dir_map_.begin();
       i != user_data_dir_map_.end();
       ++i) {
    // Delete temporary directories now to avoid using too much space in /tmp.
    if (!base::DeleteFile(i->second, true)) {
      LOG(WARNING) << "Failed to delete " << i->second.value();
    }
  }
  user_data_dir_map_.clear();

  for (std::set<std::string>::const_iterator i = test_names_set.begin();
       i != test_names_set.end();
       ++i) {
    std::string full_name(*i);

    // Make sure PRE_ tests and tests that depend on them share the same
    // data directory - based it on the test name without prefixes.
    std::string test_name_no_pre(RemoveAnyPrePrefixes(full_name));
    if (!base::ContainsKey(user_data_dir_map_, test_name_no_pre)) {
      base::FilePath temp_dir;
      CHECK(base::CreateTemporaryDirInDir(temp_dir_.GetPath(),
                                          FILE_PATH_LITERAL("d"), &temp_dir));
      user_data_dir_map_[test_name_no_pre] = temp_dir;
    }

    size_t dot_pos = full_name.find('.');
    CHECK_NE(dot_pos, std::string::npos);
    std::string test_case_name = full_name.substr(0, dot_pos);
    std::string test_name = full_name.substr(dot_pos + 1);
    std::string pre_test_name(
        test_case_name + "." + kPreTestPrefix + test_name);
    if (!base::ContainsKey(test_names_set, pre_test_name))
      tests_to_run_now.push_back(full_name);
  }

  DoRunTests(test_launcher, tests_to_run_now);

  return test_names_set.size();
}

void WrapperTestLauncherDelegate::DoRunTests(
    base::TestLauncher* test_launcher,
    const std::vector<std::string>& test_names) {
  if (test_names.empty())
    return;

  std::string test_name(test_names.front());
  std::vector<std::string> test_names_copy(
      test_names.begin() + 1, test_names.end());

  std::string test_name_no_pre(RemoveAnyPrePrefixes(test_name));

  base::CommandLine cmd_line(*base::CommandLine::ForCurrentProcess());
  base::TestLauncher::LaunchOptions test_launch_options;
  test_launch_options.flags = base::TestLauncher::USE_JOB_OBJECTS |
                              base::TestLauncher::ALLOW_BREAKAWAY_FROM_JOB;
  std::unique_ptr<TestState> test_state_ptr =
      launcher_delegate_->PreRunTest(&cmd_line, &test_launch_options);
  CHECK(launcher_delegate_->AdjustChildProcessCommandLine(
            &cmd_line, user_data_dir_map_[test_name_no_pre]));

  base::CommandLine new_cmd_line(cmd_line.GetProgram());
  base::CommandLine::SwitchMap switches = cmd_line.GetSwitches();

  // Strip out gtest_output flag because otherwise we would overwrite results
  // of the other tests.
  switches.erase(base::kGTestOutputFlag);

  for (base::CommandLine::SwitchMap::const_iterator iter = switches.begin();
       iter != switches.end(); ++iter) {
    new_cmd_line.AppendSwitchNative(iter->first, iter->second);
  }

  // Always enable disabled tests.  This method is not called with disabled
  // tests unless this flag was specified to the browser test executable.
  new_cmd_line.AppendSwitch("gtest_also_run_disabled_tests");
  new_cmd_line.AppendSwitchASCII("gtest_filter", test_name);
  new_cmd_line.AppendSwitch(kSingleProcessTestsFlag);

  char* browser_wrapper = getenv("BROWSER_WRAPPER");

  TestState* test_state = test_state_ptr.get();
  test_launcher->LaunchChildGTestProcess(
      new_cmd_line, browser_wrapper ? browser_wrapper : std::string(),
      TestTimeouts::action_max_timeout(), test_launch_options,
      base::Bind(&WrapperTestLauncherDelegate::GTestCallback,
                 base::Unretained(this), test_launcher, test_names_copy,
                 test_name, base::Passed(&test_state_ptr)),
      base::Bind(&CallChildProcessLaunched, base::Unretained(test_state)));
}

void WrapperTestLauncherDelegate::RunDependentTest(
    base::TestLauncher* test_launcher,
    const std::string test_name,
    const base::TestResult& pre_test_result) {
  if (pre_test_result.status == base::TestResult::TEST_SUCCESS) {
    // Only run the dependent test if PRE_ test succeeded.
    std::vector<std::string> test_list;
    test_list.push_back(test_name);
    DoRunTests(test_launcher, test_list);
  } else {
    // Otherwise skip the test.
    base::TestResult test_result;
    test_result.full_name = test_name;
    test_result.status = base::TestResult::TEST_SKIPPED;
    test_launcher->OnTestFinished(test_result);

    if (base::ContainsKey(dependent_test_map_, test_name)) {
      RunDependentTest(test_launcher,
                       dependent_test_map_[test_name],
                       test_result);
    }
  }
}

void WrapperTestLauncherDelegate::GTestCallback(
    base::TestLauncher* test_launcher,
    const std::vector<std::string>& test_names,
    const std::string& test_name,
    std::unique_ptr<TestState> test_state,
    int exit_code,
    const base::TimeDelta& elapsed_time,
    bool was_timeout,
    const std::string& output) {
  base::TestResult result;
  result.full_name = test_name;

  // TODO(phajdan.jr): Recognize crashes.
  if (exit_code == 0)
    result.status = base::TestResult::TEST_SUCCESS;
  else if (was_timeout)
    result.status = base::TestResult::TEST_TIMEOUT;
  else
    result.status = base::TestResult::TEST_FAILURE;

  result.elapsed_time = elapsed_time;

  result.output_snippet = GetTestOutputSnippet(result, output);

  if (base::ContainsKey(dependent_test_map_, test_name)) {
    RunDependentTest(test_launcher, dependent_test_map_[test_name], result);
  } else {
    // No other tests depend on this, we can delete the temporary directory now.
    // Do so to avoid too many temporary files using lots of disk space.
    std::string test_name_no_pre(RemoveAnyPrePrefixes(test_name));
    if (base::ContainsKey(user_data_dir_map_, test_name_no_pre)) {
      if (!base::DeleteFile(user_data_dir_map_[test_name_no_pre], true)) {
        LOG(WARNING) << "Failed to delete "
                     << user_data_dir_map_[test_name_no_pre].value();
      }
      user_data_dir_map_.erase(test_name_no_pre);
    }
  }

  test_launcher->OnTestFinished(result);

  DoRunTests(test_launcher, test_names);
}

}  // namespace

const char kHelpFlag[]   = "help";

const char kLaunchAsBrowser[] = "as-browser";

// See kManualTestPrefix above.
const char kRunManualTestsFlag[] = "run-manual";

const char kSingleProcessTestsFlag[]   = "single_process";

std::unique_ptr<TestState> TestLauncherDelegate::PreRunTest(
    base::CommandLine* command_line,
    base::TestLauncher::LaunchOptions* test_launch_options) {
  return nullptr;
}

void TestLauncherDelegate::OnDoneRunningTests() {}

TestLauncherDelegate::~TestLauncherDelegate() {
}

int LaunchTests(TestLauncherDelegate* launcher_delegate,
                int default_jobs,
                int argc,
                char** argv) {
  DCHECK(!g_launcher_delegate);
  g_launcher_delegate = launcher_delegate;

  base::CommandLine::Init(argc, argv);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kHelpFlag)) {
    PrintUsage();
    return 0;
  }

  std::unique_ptr<ContentMainDelegate> chrome_main_delegate(
      launcher_delegate->CreateContentMainDelegate());
  ContentMainParams params(chrome_main_delegate.get());

#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  InitializeSandboxInfo(&sandbox_info);

  params.instance = GetModuleHandle(NULL);
  params.sandbox_info = &sandbox_info;
#elif !defined(OS_ANDROID)
  params.argc = argc;
  params.argv = const_cast<const char**>(argv);
#endif  // defined(OS_WIN)

  if (command_line->HasSwitch(kSingleProcessTestsFlag) ||
      (command_line->HasSwitch(switches::kSingleProcess) &&
       command_line->HasSwitch(base::kGTestFilterFlag)) ||
      command_line->HasSwitch(base::kGTestListTestsFlag) ||
      command_line->HasSwitch(base::kGTestHelpFlag)) {
    g_params = &params;
    return launcher_delegate->RunTestSuite(argc, argv);
  }

#if !defined(OS_ANDROID)
  if (command_line->HasSwitch(switches::kProcessType) ||
      command_line->HasSwitch(kLaunchAsBrowser)) {
    return ContentMain(params);
  }
#endif

  base::AtExitManager at_exit;
  testing::InitGoogleTest(&argc, argv);
  TestTimeouts::Initialize();

  fprintf(stdout,
      "IMPORTANT DEBUGGING NOTE: each test is run inside its own process.\n"
      "For debugging a test inside a debugger, use the\n"
      "--gtest_filter=<your_test_name> flag along with either\n"
      "--single_process (to run the test in one launcher/browser process) or\n"
      "--single-process (to do the above, and also run Chrome in single-"
          "process mode).\n");

  base::MessageLoopForIO message_loop;
#if defined(OS_POSIX)
  base::FileDescriptorWatcher file_descriptor_watcher(&message_loop);
#endif

  // Allow the |launcher_delegate| to modify |default_jobs|.
  launcher_delegate->AdjustDefaultParallelJobs(&default_jobs);

  WrapperTestLauncherDelegate delegate(launcher_delegate);
  base::TestLauncher launcher(&delegate, default_jobs);
  const int result = launcher.Run() ? 0 : 1;
  launcher_delegate->OnDoneRunningTests();
  return result;
}

TestLauncherDelegate* GetCurrentTestLauncherDelegate() {
  return g_launcher_delegate;
}

ContentMainParams* GetContentMainParams() {
  return g_params;
}

}  // namespace content
