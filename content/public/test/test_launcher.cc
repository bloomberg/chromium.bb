// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_launcher.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/test/test_suite.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
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

// Tests with this suffix are expected to crash, so it won't count as a failure.
// A test that uses this must have a PRE_ prefix.
const char kCrashTestSuffix[] = "_CRASH";

TestLauncherDelegate* g_launcher_delegate;
}

// The environment variable name for the total number of test shards.
const char kTestTotalShards[] = "GTEST_TOTAL_SHARDS";
// The environment variable name for the test shard index.
const char kTestShardIndex[] = "GTEST_SHARD_INDEX";

// The default output file for XML output.
const base::FilePath::CharType kDefaultOutputFile[] = FILE_PATH_LITERAL(
    "test_detail.xml");

// Quit test execution after this number of tests has timed out.
const int kMaxTimeouts = 5;  // 45s timeout * (5 + 1) = 270s max run time.

namespace {

// Parses the environment variable var as an Int32.  If it is unset, returns
// default_val.  If it is set, unsets it then converts it to Int32 before
// returning it.  If unsetting or converting to an Int32 fails, print an
// error and exit with failure.
int32 Int32FromEnvOrDie(const char* const var, int32 default_val) {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string str_val;
  int32 result;
  if (!env->GetVar(var, &str_val))
    return default_val;
  if (!env->UnSetVar(var)) {
    LOG(ERROR) << "Invalid environment: we could not unset " << var << ".\n";
    exit(EXIT_FAILURE);
  }
  if (!base::StringToInt(str_val, &result)) {
    LOG(ERROR) << "Invalid environment: " << var << " is not an integer.\n";
    exit(EXIT_FAILURE);
  }
  return result;
}

// Checks whether sharding is enabled by examining the relevant
// environment variable values.  If the variables are present,
// but inconsistent (i.e., shard_index >= total_shards), prints
// an error and exits.
bool ShouldShard(int32* total_shards, int32* shard_index) {
  *total_shards = Int32FromEnvOrDie(kTestTotalShards, -1);
  *shard_index = Int32FromEnvOrDie(kTestShardIndex, -1);

  if (*total_shards == -1 && *shard_index == -1) {
    return false;
  } else if (*total_shards == -1 && *shard_index != -1) {
    LOG(ERROR) << "Invalid environment variables: you have "
               << kTestShardIndex << " = " << *shard_index
               << ", but have left " << kTestTotalShards << " unset.\n";
    exit(EXIT_FAILURE);
  } else if (*total_shards != -1 && *shard_index == -1) {
    LOG(ERROR) << "Invalid environment variables: you have "
               << kTestTotalShards << " = " << *total_shards
               << ", but have left " << kTestShardIndex << " unset.\n";
    exit(EXIT_FAILURE);
  } else if (*shard_index < 0 || *shard_index >= *total_shards) {
    LOG(ERROR) << "Invalid environment variables: we require 0 <= "
               << kTestShardIndex << " < " << kTestTotalShards
               << ", but you have " << kTestShardIndex << "=" << *shard_index
               << ", " << kTestTotalShards << "=" << *total_shards << ".\n";
    exit(EXIT_FAILURE);
  }

  return *total_shards > 1;
}

// Given the total number of shards, the shard index, and the test id, returns
// true iff the test should be run on this shard.  The test id is some arbitrary
// but unique non-negative integer assigned by this launcher to each test
// method.  Assumes that 0 <= shard_index < total_shards, which is first
// verified in ShouldShard().
bool ShouldRunTestOnShard(int total_shards, int shard_index, int test_id) {
  return (test_id % total_shards) == shard_index;
}

// A helper class to output results.
// Note: as currently XML is the only supported format by gtest, we don't
// check output format (e.g. "xml:" prefix) here and output an XML file
// unconditionally.
// Note: we don't output per-test-case or total summary info like
// total failed_test_count, disabled_test_count, elapsed_time and so on.
// Only each test (testcase element in the XML) will have the correct
// failed/disabled/elapsed_time information. Each test won't include
// detailed failure messages either.
class ResultsPrinter {
 public:
  explicit ResultsPrinter(const CommandLine& command_line);
  ~ResultsPrinter();
  void OnTestCaseStart(const char* name, int test_count) const;
  void OnTestCaseEnd() const;

  void OnTestEnd(const char* name, const char* case_name, bool run,
                 bool failed, bool failure_ignored, double elapsed_time) const;
 private:
  FILE* out_;

  DISALLOW_COPY_AND_ASSIGN(ResultsPrinter);
};

ResultsPrinter::ResultsPrinter(const CommandLine& command_line) : out_(NULL) {
  if (!command_line.HasSwitch(kGTestOutputFlag))
    return;
  std::string flag = command_line.GetSwitchValueASCII(kGTestOutputFlag);
  size_t colon_pos = flag.find(':');
  base::FilePath path;
  if (colon_pos != std::string::npos) {
    base::FilePath flag_path =
        command_line.GetSwitchValuePath(kGTestOutputFlag);
    base::FilePath::StringType path_string = flag_path.value();
    path = base::FilePath(path_string.substr(colon_pos + 1));
    // If the given path ends with '/', consider it is a directory.
    // Note: This does NOT check that a directory (or file) actually exists
    // (the behavior is same as what gtest does).
    if (file_util::EndsWithSeparator(path)) {
      base::FilePath executable = command_line.GetProgram().BaseName();
      path = path.Append(executable.ReplaceExtension(
          base::FilePath::StringType(FILE_PATH_LITERAL("xml"))));
    }
  }
  if (path.value().empty())
    path = base::FilePath(kDefaultOutputFile);
  base::FilePath dir_name = path.DirName();
  if (!file_util::DirectoryExists(dir_name)) {
    LOG(WARNING) << "The output directory does not exist. "
                 << "Creating the directory: " << dir_name.value();
    // Create the directory if necessary (because the gtest does the same).
    file_util::CreateDirectory(dir_name);
  }
  out_ = file_util::OpenFile(path, "w");
  if (!out_) {
    LOG(ERROR) << "Cannot open output file: "
               << path.value() << ".";
    return;
  }
  fprintf(out_, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(out_, "<testsuites name=\"AllTests\" tests=\"\" failures=\"\""
          " disabled=\"\" errors=\"\" time=\"\">\n");
}

ResultsPrinter::~ResultsPrinter() {
  if (!out_)
    return;
  fprintf(out_, "</testsuites>\n");
  fclose(out_);
}

void ResultsPrinter::OnTestCaseStart(const char* name, int test_count) const {
  if (!out_)
    return;
  fprintf(out_, "  <testsuite name=\"%s\" tests=\"%d\" failures=\"\""
          " disabled=\"\" errors=\"\" time=\"\">\n", name, test_count);
}

void ResultsPrinter::OnTestCaseEnd() const {
  if (!out_)
    return;
  fprintf(out_, "  </testsuite>\n");
}

void ResultsPrinter::OnTestEnd(const char* name,
                               const char* case_name,
                               bool run,
                               bool failed,
                               bool failure_ignored,
                               double elapsed_time) const {
  if (!out_)
    return;
  fprintf(out_, "    <testcase name=\"%s\" status=\"%s\" time=\"%.3f\""
          " classname=\"%s\"",
          name, run ? "run" : "notrun", elapsed_time / 1000.0, case_name);
  if (!failed) {
    fprintf(out_, " />\n");
    return;
  }
  fprintf(out_, ">\n");
  fprintf(out_, "      <failure message=\"\" type=\"\"%s></failure>\n",
          failure_ignored ? " ignored=\"true\"" : "");
  fprintf(out_, "    </testcase>\n");
}

class TestCasePrinterHelper {
 public:
  TestCasePrinterHelper(const ResultsPrinter& printer,
                        const char* name,
                        int total_test_count)
      : printer_(printer) {
    printer_.OnTestCaseStart(name, total_test_count);
  }
  ~TestCasePrinterHelper() {
    printer_.OnTestCaseEnd();
  }
 private:
  const ResultsPrinter& printer_;

  DISALLOW_COPY_AND_ASSIGN(TestCasePrinterHelper);
};

// For a basic pattern matching for gtest_filter options.  (Copied from
// gtest.cc, see the comment below and http://crbug.com/44497)
bool PatternMatchesString(const char* pattern, const char* str) {
  switch (*pattern) {
    case '\0':
    case ':':  // Either ':' or '\0' marks the end of the pattern.
      return *str == '\0';
    case '?':  // Matches any single character.
      return *str != '\0' && PatternMatchesString(pattern + 1, str + 1);
    case '*':  // Matches any string (possibly empty) of characters.
      return (*str != '\0' && PatternMatchesString(pattern, str + 1)) ||
          PatternMatchesString(pattern + 1, str);
    default:  // Non-special character.  Matches itself.
      return *pattern == *str &&
          PatternMatchesString(pattern + 1, str + 1);
  }
}

// TODO(phajdan.jr): Avoid duplicating gtest code. (http://crbug.com/44497)
// For basic pattern matching for gtest_filter options.  (Copied from
// gtest.cc)
bool MatchesFilter(const std::string& name, const std::string& filter) {
  const char *cur_pattern = filter.c_str();
  for (;;) {
    if (PatternMatchesString(cur_pattern, name.c_str())) {
      return true;
    }

    // Finds the next pattern in the filter.
    cur_pattern = strchr(cur_pattern, ':');

    // Returns if no more pattern can be found.
    if (cur_pattern == NULL) {
      return false;
    }

    // Skips the pattern separater (the ':' character).
    cur_pattern++;
  }
}

int RunTestInternal(const testing::TestCase* test_case,
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
        int exit_code = RunTestInternal(test_case, pre_test_name, command_line,
                                        default_timeout, was_timeout);
        if (exit_code != 0 &&
            !EndsWith(pre_test_name, kCrashTestSuffix, true)) {
          return exit_code;
        }
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
int RunTest(TestLauncherDelegate* launcher_delegate,
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
  switches.erase(kGTestOutputFlag);

  // Strip out gtest_repeat flag because we can only run one test in the child
  // process (restarting the browser in the same process is illegal after it
  // has been shut down and will actually crash).
  switches.erase(kGTestRepeatFlag);

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

  return RunTestInternal(
      test_case, test_name, &new_cmd_line, default_timeout, was_timeout);
}

bool RunTests(TestLauncherDelegate* launcher_delegate,
              bool should_shard,
              int total_shards,
              int shard_index) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  DCHECK(!command_line->HasSwitch(kGTestListTestsFlag));

  testing::UnitTest* const unit_test = testing::UnitTest::GetInstance();

  std::string filter = command_line->GetSwitchValueASCII(kGTestFilterFlag);

  // Split --gtest_filter at '-', if there is one, to separate into
  // positive filter and negative filter portions.
  std::string positive_filter = filter;
  std::string negative_filter = "";
  size_t dash_pos = filter.find('-');
  if (dash_pos != std::string::npos) {
    positive_filter = filter.substr(0, dash_pos);  // Everything up to the dash.
    negative_filter = filter.substr(dash_pos + 1); // Everything after the dash.
  }

  int num_runnable_tests = 0;
  int test_run_count = 0;
  int timeout_count = 0;
  std::vector<std::string> failed_tests;
  std::set<std::string> ignored_tests;

  ResultsPrinter printer(*command_line);
  for (int i = 0; i < unit_test->total_test_case_count(); ++i) {
    const testing::TestCase* test_case = unit_test->GetTestCase(i);
    TestCasePrinterHelper helper(printer, test_case->name(),
                                 test_case->total_test_count());
    for (int j = 0; j < test_case->total_test_count(); ++j) {
      const testing::TestInfo* test_info = test_case->GetTestInfo(j);
      std::string test_name = test_info->test_case_name();
      test_name.append(".");
      test_name.append(test_info->name());

      // Skip our special test so it's not run twice. That confuses the log
      // parser.
      if (test_name == launcher_delegate->GetEmptyTestName())
        continue;

      // Skip disabled tests.
      if (test_name.find("DISABLED") != std::string::npos &&
          !command_line->HasSwitch(kGTestRunDisabledTestsFlag)) {
        printer.OnTestEnd(test_info->name(), test_case->name(),
                          false, false, false, 0);
        continue;
      }

      if (StartsWithASCII(test_info->name(), kPreTestPrefix, true))
        continue;

      if (StartsWithASCII(test_info->name(), kManualTestPrefix, true) &&
          !command_line->HasSwitch(kRunManualTestsFlag)) {
        continue;
      }

      // Skip the test that doesn't match the filter string (if given).
      if ((!positive_filter.empty() &&
           !MatchesFilter(test_name, positive_filter)) ||
          MatchesFilter(test_name, negative_filter)) {
        printer.OnTestEnd(test_info->name(), test_case->name(),
                          false, false, false, 0);
        continue;
      }

      // Decide if this test should be run.
      bool should_run = true;
      if (should_shard) {
        should_run = ShouldRunTestOnShard(total_shards, shard_index,
                                          num_runnable_tests);
      }
      num_runnable_tests += 1;
      // If sharding is enabled and the test should not be run, skip it.
      if (!should_run) {
        continue;
      }

      base::TimeTicks start_time = base::TimeTicks::Now();
      ++test_run_count;
      bool was_timeout = false;
      int exit_code = RunTest(launcher_delegate,
                              test_case,
                              test_name,
                              TestTimeouts::action_max_timeout(),
                              &was_timeout);
      if (exit_code == 0) {
        // Test passed.
        printer.OnTestEnd(
            test_info->name(), test_case->name(), true, false,
            false,
            (base::TimeTicks::Now() - start_time).InMillisecondsF());
      } else {
        failed_tests.push_back(test_name);

        bool ignore_failure = false;
        printer.OnTestEnd(
            test_info->name(), test_case->name(), true, true,
            ignore_failure,
            (base::TimeTicks::Now() - start_time).InMillisecondsF());
        if (ignore_failure)
          ignored_tests.insert(test_name);

        if (was_timeout)
          ++timeout_count;
      }

      if (timeout_count > kMaxTimeouts) {
        printf("More than %d timeouts, aborting test case\n", kMaxTimeouts);
        break;
      }
    }
    if (timeout_count > kMaxTimeouts) {
      printf("More than %d timeouts, aborting test\n", kMaxTimeouts);
      break;
    }
  }

  printf("%d test%s run\n", test_run_count, test_run_count > 1 ? "s" : "");
  printf("%d test%s failed (%d ignored)\n",
         static_cast<int>(failed_tests.size()),
         failed_tests.size() != 1 ? "s" : "",
         static_cast<int>(ignored_tests.size()));
  if (failed_tests.size() == ignored_tests.size())
    return true;

  printf("Failing tests:\n");
  for (std::vector<std::string>::const_iterator iter = failed_tests.begin();
       iter != failed_tests.end(); ++iter) {
    bool is_ignored = ignored_tests.find(*iter) != ignored_tests.end();
    printf("%s%s\n", iter->c_str(), is_ignored ? " (ignored)" : "");
  }

  return false;
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

}  // namespace

// The following is kept for historical reasons (so people that are used to
// using it don't get surprised).
const char kChildProcessFlag[]   = "child";

const char kGTestFilterFlag[] = "gtest_filter";
const char kGTestHelpFlag[]   = "gtest_help";
const char kGTestListTestsFlag[] = "gtest_list_tests";
const char kGTestRepeatFlag[] = "gtest_repeat";
const char kGTestRunDisabledTestsFlag[] = "gtest_also_run_disabled_tests";
const char kGTestOutputFlag[] = "gtest_output";

const char kHelpFlag[]   = "help";

const char kLaunchAsBrowser[] = "as-browser";

// See kManualTestPrefix above.
const char kRunManualTestsFlag[] = "run-manual";

const char kSingleProcessTestsFlag[]   = "single_process";

const char kWarmupFlag[] = "warmup";


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
       command_line->HasSwitch(kGTestFilterFlag)) ||
      command_line->HasSwitch(kGTestListTestsFlag) ||
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

  base::AtExitManager at_exit;

  int32 total_shards;
  int32 shard_index;
  bool should_shard = ShouldShard(&total_shards, &shard_index);

  fprintf(stdout,
      "Starting tests...\n"
      "IMPORTANT DEBUGGING NOTE: each test is run inside its own process.\n"
      "For debugging a test inside a debugger, use the\n"
      "--gtest_filter=<your_test_name> flag along with either\n"
      "--single_process (to run the test in one launcher/browser process) or\n"
      "--single-process (to do the above, and also run Chrome in single-"
      "process mode).\n");

  testing::InitGoogleTest(&argc, argv);
  TestTimeouts::Initialize();
  int exit_code = 0;

  std::string empty_test = launcher_delegate->GetEmptyTestName();
  if (!empty_test.empty()) {
    // Make sure the entire browser code is loaded into memory. Reading it
    // from disk may be slow on a busy bot, and can easily exceed the default
    // timeout causing flaky test failures. Use an empty test that only starts
    // and closes a browser with a long timeout to avoid those problems.
    // NOTE: We don't do this when specifying a filter because this slows down
    // the common case of running one test locally, and also on trybots when
    // sharding as this one test runs ~200 times and wastes a few minutes.
    bool warmup = command_line->HasSwitch(kWarmupFlag);
    bool has_filter = command_line->HasSwitch(kGTestFilterFlag);
    if (warmup || (!should_shard && !has_filter)) {
      exit_code = RunTest(launcher_delegate,
                          NULL,
                          empty_test,
                          TestTimeouts::large_test_timeout(),
                          NULL);
      if (exit_code != 0 || warmup)
        return exit_code;
    }
  }

  int cycles = 1;
  if (command_line->HasSwitch(kGTestRepeatFlag)) {
    base::StringToInt(command_line->GetSwitchValueASCII(kGTestRepeatFlag),
                      &cycles);
  }

  while (cycles != 0) {
    if (!RunTests(launcher_delegate,
                  should_shard,
                  total_shards,
                  shard_index)) {
      exit_code = 1;
      break;
    }

    // Special value "-1" means "repeat indefinitely".
    if (cycles != -1)
      cycles--;
  }
  return exit_code;
}

TestLauncherDelegate* GetCurrentTestLauncherDelegate() {
  return g_launcher_delegate;
}

}  // namespace content
