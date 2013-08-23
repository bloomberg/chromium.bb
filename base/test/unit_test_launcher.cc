// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/unit_test_launcher.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/test/gtest_xml_util.h"
#include "base/test/test_launcher.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// This constant controls how many tests are run in a single batch.
const size_t kTestBatchLimit = 10;

// Flag to enable the new launcher logic.
// TODO(phajdan.jr): Remove it, http://crbug.com/236893 .
const char kBraveNewTestLauncherFlag[] = "brave-new-test-launcher";

// Flag to run all tests in a single process.
const char kSingleProcessTestsFlag[] = "single-process-tests";

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
 private:
  struct TestLaunchInfo {
    std::string GetFullName() const {
      return test_case_name + "." + test_name;
    }

    std::string test_case_name;
    std::string test_name;
    TestResultCallback callback;
  };

  virtual bool ShouldRunTest(const testing::TestCase* test_case,
                             const testing::TestInfo* test_info) OVERRIDE {
    // There is no additional logic to disable specific tests.
    return true;
  }

  virtual void RunTest(const testing::TestCase* test_case,
                       const testing::TestInfo* test_info,
                       const TestResultCallback& callback) OVERRIDE {
    TestLaunchInfo launch_info;
    launch_info.test_case_name = test_case->name();
    launch_info.test_name = test_info->name();
    launch_info.callback = callback;
    tests_.push_back(launch_info);

    // Run tests in batches no larger than the limit.
    if (tests_.size() >= kTestBatchLimit)
      RunRemainingTests();
  }

  virtual void RunRemainingTests() OVERRIDE {
    if (tests_.empty())
      return;

    // Create a dedicated temporary directory to store the xml result data
    // per run to ensure clean state and make it possible to launch multiple
    // processes in parallel.
    base::FilePath output_file;
    base::ScopedTempDir temp_dir;
    CHECK(temp_dir.CreateUniqueTempDir());
    output_file = temp_dir.path().AppendASCII("test_results.xml");

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
        test_names.size() * TestTimeouts::action_timeout();

    // TODO(phajdan.jr): Distinguish between test failures and crashes.
    bool was_timeout = false;
    int exit_code = LaunchChildGTestProcess(cmd_line,
                                            std::string(),
                                            timeout,
                                            &was_timeout);

    std::vector<TestLaunchInfo> tests_to_relaunch_after_crash;
    ProcessTestResults(output_file, exit_code, &tests_to_relaunch_after_crash);

    tests_ = tests_to_relaunch_after_crash;
  }

  void ProcessTestResults(
      const base::FilePath& output_file,
      int exit_code,
      std::vector<TestLaunchInfo>* tests_to_relaunch_after_crash) {
    std::vector<TestResult> test_results;
    bool crashed = false;
    bool have_test_results =
        ProcessGTestOutput(output_file, &test_results, &crashed);

    if (have_test_results) {
      // TODO(phajdan.jr): Check for duplicates and mismatches between
      // the results we got from XML file and tests we intended to run.
      std::map<std::string, TestResult> results_map;
      for (size_t i = 0; i < test_results.size(); i++)
        results_map[test_results[i].GetFullName()] = test_results[i];

      bool had_crashed_test = false;

      for (size_t i = 0; i < tests_.size(); i++) {
        if (ContainsKey(results_map, tests_[i].GetFullName())) {
          TestResult result = results_map[tests_[i].GetFullName()];
          if (result.crashed)
            had_crashed_test = true;
          tests_[i].callback.Run(results_map[tests_[i].GetFullName()]);
        } else if (had_crashed_test) {
          tests_to_relaunch_after_crash->push_back(tests_[i]);
        } else {
          // TODO(phajdan.jr): Explicitly pass the info that the test didn't
          // run for a mysterious reason.
          LOG(ERROR) << "no test result for " << tests_[i].GetFullName();
          TestResult test_result;
          test_result.test_case_name = tests_[i].test_case_name;
          test_result.test_name = tests_[i].test_name;
          test_result.success = false;
          test_result.crashed = false;
          tests_[i].callback.Run(test_result);
        }
      }

      // TODO(phajdan.jr): Handle the case where processing XML output
      // indicates a crash but none of the test results is marked as crashing.

      // TODO(phajdan.jr): Handle the case where the exit code is non-zero
      // but results file indicates that all tests passed (e.g. crash during
      // shutdown).
    } else {
      // We do not have reliable details about test results (parsing test
      // stdout is known to be unreliable), apply the executable exit code
      // to all tests.
      // TODO(phajdan.jr): Be smarter about this, e.g. retry each test
      // individually.
      for (size_t i = 0; i < tests_.size(); i++) {
        TestResult test_result;
        test_result.test_case_name = tests_[i].test_case_name;
        test_result.test_name = tests_[i].test_name;
        test_result.success = (exit_code == 0);
        test_result.crashed = false;
        tests_[i].callback.Run(test_result);
      }
    }
  }

  std::vector<TestLaunchInfo> tests_;
};

}  // namespace

int LaunchUnitTests(int argc,
                    char** argv,
                    const RunTestSuiteCallback& run_test_suite) {
  CommandLine::Init(argc, argv);
  if (CommandLine::ForCurrentProcess()->HasSwitch(kSingleProcessTestsFlag) ||
      !CommandLine::ForCurrentProcess()->HasSwitch(kBraveNewTestLauncherFlag)) {
    return run_test_suite.Run();
  }

  base::TimeTicks start_time(base::TimeTicks::Now());

  fprintf(stdout,
      "Starting tests...\n"
      "IMPORTANT DEBUGGING NOTE: batches of tests are run inside their own \n"
      "process. For debugging a test inside a debugger, use the\n"
      "--gtest_filter=<your_test_name> flag along with \n"
      "--single-process-tests.\n");
  fflush(stdout);

  testing::InitGoogleTest(&argc, argv);
  TestTimeouts::Initialize();

  base::UnitTestLauncherDelegate delegate;
  int exit_code = base::LaunchTests(&delegate, argc, argv);

  fprintf(stdout,
          "Tests took %" PRId64 " seconds.\n",
          (base::TimeTicks::Now() - start_time).InSeconds());
  fflush(stdout);

  return exit_code;
}

}  // namespace base
