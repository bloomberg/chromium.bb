// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/unit_test_launcher.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/test/gtest_xml_util.h"
#include "base/test/launcher/parallel_test_launcher.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_checker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// This constant controls how many tests are run in a single batch by default.
const size_t kDefaultTestBatchLimit = 10;

const char kHelpFlag[] = "help";

// Flag to enable the new launcher logic.
// TODO(phajdan.jr): Remove it, http://crbug.com/236893 .
const char kBraveNewTestLauncherFlag[] = "brave-new-test-launcher";

// Flag to run all tests in a single process.
const char kSingleProcessTestsFlag[] = "single-process-tests";

void PrintUsage() {
  fprintf(stdout,
          "Runs tests using the gtest framework, each batch of tests being\n"
          "run in their own process. Supported command-line flags:\n"
          "\n"
          "  --single-process-tests\n"
          "    Runs the tests and the launcher in the same process. Useful\n"
          "    for debugging a specific test in a debugger.\n"
          "  --test-launcher-jobs=N\n"
          "    Sets the number of parallel test jobs to N.\n"
          "  --test-launcher-batch-limit=N\n"
          "    Sets the limit of test batch to run in a single process to N.\n"
          "  --gtest_filter=...\n"
          "    Runs a subset of tests (see --gtest_help for more info).\n"
          "  --help\n"
          "    Shows this message.\n"
          "  --gtest_help\n"
          "    Shows the gtest help message.\n");
  fflush(stdout);
}

// Returns command line for child GTest process based on the command line
// of current process. |test_names| is a vector of test full names
// (e.g. "A.B"), |output_file| is path to the GTest XML output file.
CommandLine GetCommandLineForChildGTestProcess(
    const std::vector<std::string>& test_names,
    const base::FilePath& output_file) {
  CommandLine new_cmd_line(*CommandLine::ForCurrentProcess());

  new_cmd_line.AppendSwitchPath(switches::kTestLauncherOutput, output_file);
  new_cmd_line.AppendSwitchASCII(kGTestFilterFlag, JoinString(test_names, ":"));
  new_cmd_line.AppendSwitch(kSingleProcessTestsFlag);
  new_cmd_line.AppendSwitch(kBraveNewTestLauncherFlag);

  return new_cmd_line;
}

class UnitTestLauncherDelegate : public TestLauncherDelegate {
 public:
  UnitTestLauncherDelegate(size_t jobs, size_t batch_limit)
      : parallel_launcher_(jobs),
        batch_limit_(batch_limit) {
  }

  virtual ~UnitTestLauncherDelegate() {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

 private:
  struct TestLaunchInfo {
    std::string GetFullName() const {
      return test_case_name + "." + test_name;
    }

    std::string test_case_name;
    std::string test_name;
    TestResultCallback callback;
  };

  virtual void OnTestIterationStarting() OVERRIDE {
    // Nothing to do.
  }

  virtual std::string GetTestNameForFiltering(
      const testing::TestCase* test_case,
      const testing::TestInfo* test_info) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());

    return std::string(test_case->name()) + "." + test_info->name();
  }

  virtual bool ShouldRunTest(const testing::TestCase* test_case,
                             const testing::TestInfo* test_info) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());

    // There is no additional logic to disable specific tests.
    return true;
  }

  virtual void RunTest(const testing::TestCase* test_case,
                       const testing::TestInfo* test_info,
                       const TestResultCallback& callback) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());

    TestLaunchInfo launch_info;
    launch_info.test_case_name = test_case->name();
    launch_info.test_name = test_info->name();
    launch_info.callback = callback;
    tests_.push_back(launch_info);

    // Run tests in batches no larger than the limit.
    if (tests_.size() >= batch_limit_)
      RunRemainingTests();
  }

  virtual void RunRemainingTests() OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (tests_.empty())
      return;

    // Create a dedicated temporary directory to store the xml result data
    // per run to ensure clean state and make it possible to launch multiple
    // processes in parallel.
    base::FilePath output_file;
    CHECK(file_util::CreateNewTempDirectory(FilePath::StringType(),
                                            &output_file));
    output_file = output_file.AppendASCII("test_results.xml");

    std::vector<std::string> test_names;
    for (size_t i = 0; i < tests_.size(); i++)
      test_names.push_back(tests_[i].GetFullName());

    CommandLine cmd_line(
        GetCommandLineForChildGTestProcess(test_names, output_file));

    // Adjust the timeout depending on how many tests we're running
    // (note that e.g. the last batch of tests will be smaller).
    // TODO(phajdan.jr): Consider an adaptive timeout, which can change
    // depending on how many tests ran and how many remain.
    // Note: do NOT parse child's stdout to do that, it's known to be
    // unreliable (e.g. buffering issues can mix up the output).
    base::TimeDelta timeout =
        test_names.size() * TestTimeouts::test_launcher_timeout();

    parallel_launcher_.LaunchChildGTestProcess(
        cmd_line,
        std::string(),
        timeout,
        Bind(&UnitTestLauncherDelegate::GTestCallback,
             base::Unretained(this),
             tests_,
             output_file));
    tests_.clear();
  }

  void GTestCallback(const std::vector<TestLaunchInfo>& tests,
                     const FilePath& output_file,
                     int exit_code,
                     const TimeDelta& elapsed_time,
                     bool was_timeout,
                     const std::string& output) {
    DCHECK(thread_checker_.CalledOnValidThread());
    std::vector<TestLaunchInfo> tests_to_relaunch_after_interruption;
    bool called_any_callbacks =
        ProcessTestResults(tests,
                           output_file,
                           output,
                           exit_code,
                           was_timeout,
                           &tests_to_relaunch_after_interruption);

    for (size_t i = 0; i < tests_to_relaunch_after_interruption.size(); i++)
      tests_.push_back(tests_to_relaunch_after_interruption[i]);
    RunRemainingTests();

    if (called_any_callbacks)
      parallel_launcher_.ResetOutputWatchdog();

    // The temporary file's directory is also temporary.
    DeleteFile(output_file.DirName(), true);
  }

  static bool ProcessTestResults(
      const std::vector<TestLaunchInfo>& tests,
      const base::FilePath& output_file,
      const std::string& output,
      int exit_code,
      bool was_timeout,
      std::vector<TestLaunchInfo>* tests_to_relaunch_after_interruption) {
    std::vector<TestResult> test_results;
    bool crashed = false;
    bool have_test_results =
        ProcessGTestOutput(output_file, &test_results, &crashed);

    bool called_any_callback = false;

    if (have_test_results) {
      // TODO(phajdan.jr): Check for duplicates and mismatches between
      // the results we got from XML file and tests we intended to run.
      std::map<std::string, TestResult> results_map;
      for (size_t i = 0; i < test_results.size(); i++)
        results_map[test_results[i].GetFullName()] = test_results[i];

      bool had_interrupted_test = false;

      for (size_t i = 0; i < tests.size(); i++) {
        if (ContainsKey(results_map, tests[i].GetFullName())) {
          TestResult test_result = results_map[tests[i].GetFullName()];
          if (test_result.status == TestResult::TEST_CRASH) {
            had_interrupted_test = true;

            if (was_timeout) {
              // Fix up the test status: we forcibly kill the child process
              // after the timeout, so from XML results it looks just like
              // a crash.
              test_result.status = TestResult::TEST_TIMEOUT;
            }
          } else if (test_result.status == TestResult::TEST_SUCCESS ||
                     test_result.status == TestResult::TEST_FAILURE) {
            // We run multiple tests in a batch with a timeout applied
            // to the entire batch. It is possible that with other tests
            // running quickly some tests take longer than the per-test timeout.
            // For consistent handling of tests independent of order and other
            // factors, mark them as timing out.
            if (test_result.elapsed_time >
                TestTimeouts::test_launcher_timeout()) {
              test_result.status = TestResult::TEST_TIMEOUT;
            }
          }
          test_result.output_snippet =
              GetTestOutputSnippet(test_result, output);
          tests[i].callback.Run(test_result);
          called_any_callback = true;
        } else if (had_interrupted_test) {
          tests_to_relaunch_after_interruption->push_back(tests[i]);
        } else {
          // TODO(phajdan.jr): Explicitly pass the info that the test didn't
          // run for a mysterious reason.
          LOG(ERROR) << "no test result for " << tests[i].GetFullName();
          TestResult test_result;
          test_result.test_case_name = tests[i].test_case_name;
          test_result.test_name = tests[i].test_name;
          test_result.status = TestResult::TEST_UNKNOWN;
          test_result.output_snippet =
              GetTestOutputSnippet(test_result, output);
          tests[i].callback.Run(test_result);
          called_any_callback = true;
        }
      }

      // TODO(phajdan.jr): Handle the case where processing XML output
      // indicates a crash but none of the test results is marked as crashing.

      // TODO(phajdan.jr): Handle the case where the exit code is non-zero
      // but results file indicates that all tests passed (e.g. crash during
      // shutdown).
    } else {
      fprintf(stdout,
              "Failed to get out-of-band test success data, "
              "dumping full stdio below:\n%s\n",
              output.c_str());
      fflush(stdout);

      // We do not have reliable details about test results (parsing test
      // stdout is known to be unreliable), apply the executable exit code
      // to all tests.
      // TODO(phajdan.jr): Be smarter about this, e.g. retry each test
      // individually.
      for (size_t i = 0; i < tests.size(); i++) {
        TestResult test_result;
        test_result.test_case_name = tests[i].test_case_name;
        test_result.test_name = tests[i].test_name;
        test_result.status = TestResult::TEST_UNKNOWN;
        tests[i].callback.Run(test_result);
        called_any_callback = true;
      }
    }

    return called_any_callback;
  }

  ThreadChecker thread_checker_;

  ParallelTestLauncher parallel_launcher_;

  // Maximum number of tests to run in a single batch.
  size_t batch_limit_;

  std::vector<TestLaunchInfo> tests_;
};

bool GetSwitchValueAsInt(const std::string& switch_name, int* result) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switch_name))
    return true;

  std::string switch_value =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switch_name);
  if (!StringToInt(switch_value, result) || *result < 1) {
    LOG(ERROR) << "Invalid value for " << switch_name << ": " << switch_value;
    return false;
  }

  return true;
}

}  // namespace

int LaunchUnitTests(int argc,
                    char** argv,
                    const RunTestSuiteCallback& run_test_suite) {
  CommandLine::Init(argc, argv);
  if (CommandLine::ForCurrentProcess()->HasSwitch(kGTestHelpFlag) ||
      CommandLine::ForCurrentProcess()->HasSwitch(kSingleProcessTestsFlag) ||
      !CommandLine::ForCurrentProcess()->HasSwitch(kBraveNewTestLauncherFlag)) {
    return run_test_suite.Run();
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(kHelpFlag)) {
    PrintUsage();
    return 0;
  }

  base::TimeTicks start_time(base::TimeTicks::Now());

  testing::InitGoogleTest(&argc, argv);
  TestTimeouts::Initialize();

  int jobs = SysInfo::NumberOfProcessors();
  if (!GetSwitchValueAsInt(switches::kTestLauncherJobs, &jobs))
    return 1;

  int batch_limit = kDefaultTestBatchLimit;
  if (!GetSwitchValueAsInt(switches::kTestLauncherBatchLimit, &batch_limit))
    return 1;

  fprintf(stdout,
          "Starting tests (using %d parallel jobs)...\n"
          "IMPORTANT DEBUGGING NOTE: batches of tests are run inside their\n"
          "own process. For debugging a test inside a debugger, use the\n"
          "--gtest_filter=<your_test_name> flag along with\n"
          "--single-process-tests.\n", jobs);
  fflush(stdout);

  MessageLoopForIO message_loop;

  base::UnitTestLauncherDelegate delegate(jobs, batch_limit);
  int exit_code = base::LaunchTests(&delegate, argc, argv);

  fprintf(stdout,
          "Tests took %" PRId64 " seconds.\n",
          (base::TimeTicks::Now() - start_time).InSeconds());
  fflush(stdout);

  return exit_code;
}

}  // namespace base
