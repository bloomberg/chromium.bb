// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_LAUNCHER_TEST_LAUNCHER_H_
#define BASE_TEST_LAUNCHER_TEST_LAUNCHER_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/test/launcher/test_result.h"
#include "base/test/launcher/test_results_tracker.h"
#include "base/time/time.h"

class CommandLine;

namespace testing {
class TestCase;
class TestInfo;
}

namespace base {

struct LaunchOptions;
class TestLauncher;

// Constants for GTest command-line flags.
extern const char kGTestFilterFlag[];
extern const char kGTestHelpFlag[];
extern const char kGTestListTestsFlag[];
extern const char kGTestRepeatFlag[];
extern const char kGTestRunDisabledTestsFlag[];
extern const char kGTestOutputFlag[];

// Interface for use with LaunchTests that abstracts away exact details
// which tests and how are run.
class TestLauncherDelegate {
 public:
  // Called at the start of each test iteration.
  virtual void OnTestIterationStarting() = 0;

  // Called to get a test name for filtering purposes. Usually it's
  // test case's name and test's name joined by a dot (e.g.
  // "TestCaseName.TestName").
  // TODO(phajdan.jr): Remove after transitioning away from run_test_cases.py,
  // http://crbug.com/236893 .
  virtual std::string GetTestNameForFiltering(
      const testing::TestCase* test_case,
      const testing::TestInfo* test_info) = 0;

  // Called before a test is considered for running. If it returns false,
  // the test is not run. If it returns true, the test will be run provided
  // it is part of the current shard.
  virtual bool ShouldRunTest(const testing::TestCase* test_case,
                             const testing::TestInfo* test_info) = 0;

  // Called to make the delegate run the specified tests. The delegate must
  // call |test_launcher|'s OnTestFinished method once per every test in
  // |test_names|, regardless of its success.
  virtual void RunTests(TestLauncher* test_launcher,
                        const std::vector<std::string>& test_names) = 0;

  // Called to make the delegate retry the specified tests. The delegate must
  // return the number of actual tests it's going to retry (can be smaller,
  // equal to, or larger than size of |test_names|). It must also call
  // |test_launcher|'s OnTestFinished method once per every retried test,
  // regardless of its success.
  virtual size_t RetryTests(TestLauncher* test_launcher,
                            const std::vector<std::string>& test_names) = 0;

 protected:
  virtual ~TestLauncherDelegate();
};

// Launches tests using a TestLauncherDelegate.
class TestLauncher {
 public:
  explicit TestLauncher(TestLauncherDelegate* launcher_delegate);
  ~TestLauncher();

  // Runs the launcher. Must be called at most once.
  bool Run(int argc, char** argv) WARN_UNUSED_RESULT;

  // Called when a test has finished running.
  void OnTestFinished(const TestResult& result);

 private:
  bool Init() WARN_UNUSED_RESULT;

  // Runs all tests in current iteration. Uses callbacks to communicate success.
  void RunTests();

  void RunTestIteration();

  void OnAllTestsStarted();

  TestLauncherDelegate* launcher_delegate_;

  // Support for outer sharding, just like gtest does.
  int32 total_shards_;  // Total number of outer shards, at least one.
  int32 shard_index_;   // Index of shard the launcher is to run.

  int cycles_;  // Number of remaining test itreations, or -1 for infinite.

  // Test filters (empty means no filter). Entries are separated by colons.
  std::string positive_test_filter_;
  std::string negative_test_filter_;

  // Number of tests started in this iteration.
  size_t test_started_count_;

  // Number of tests finished in this iteration.
  size_t test_finished_count_;

  // Number of tests successfully finished in this iteration.
  size_t test_success_count_;

  // Number of retries in this iteration.
  size_t retry_count_;

  // Maximum number of retries per iteration.
  size_t retry_limit_;

  // Tests to retry in this iteration.
  std::set<std::string> tests_to_retry_;

  // Result to be returned from Run.
  bool run_result_;

  TestResultsTracker results_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TestLauncher);
};

// Extract part from |full_output| that applies to |result|.
std::string GetTestOutputSnippet(const TestResult& result,
                                 const std::string& full_output);

// Launches a child process (assumed to be gtest-based binary)
// using |command_line|. If |wrapper| is not empty, it is prepended
// to the final command line. If the child process is still running
// after |timeout|, it is terminated and |*was_timeout| is set to true.
int LaunchChildGTestProcess(const CommandLine& command_line,
                            const std::string& wrapper,
                            base::TimeDelta timeout,
                            bool* was_timeout);

// Returns command line command line after gtest-specific processing
// and applying |wrapper|.
CommandLine PrepareCommandLineForGTest(const CommandLine& command_line,
                                       const std::string& wrapper);

// Launches a child process using |command_line|. If the child process is still
// running after |timeout|, it is terminated and |*was_timeout| is set to true.
// Returns exit code of the process.
int LaunchChildTestProcessWithOptions(const CommandLine& command_line,
                                      const LaunchOptions& options,
                                      base::TimeDelta timeout,
                                      bool* was_timeout);

}  // namespace base

#endif  // BASE_TEST_LAUNCHER_TEST_LAUNCHER_H_
