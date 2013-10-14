// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_LAUNCHER_TEST_RESULTS_TRACKER_H_
#define BASE_TEST_LAUNCHER_TEST_RESULTS_TRACKER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/test/launcher/test_launcher.h"
#include "base/threading/thread_checker.h"

class CommandLine;

namespace base {

class FilePath;

// A helper class to output results.
// Note: as currently XML is the only supported format by gtest, we don't
// check output format (e.g. "xml:" prefix) here and output an XML file
// unconditionally.
// Note: we don't output per-test-case or total summary info like
// total failed_test_count, disabled_test_count, elapsed_time and so on.
// Only each test (testcase element in the XML) will have the correct
// failed/disabled/elapsed_time information. Each test won't include
// detailed failure messages either.
class TestResultsTracker {
 public:
  typedef Callback<void(bool)> TestsResultCallback;

  TestResultsTracker();
  ~TestResultsTracker();

  // Initialize the result tracker. Must be called exactly once before
  // calling any other methods. Returns true on success.
  bool Init(const CommandLine& command_line) WARN_UNUSED_RESULT;

  // Called when a test iteration is starting. |callback| will be called
  // be the result tracker at the end of that iteration.
  void OnTestIterationStarting(const TestsResultCallback& callback);

  // Called when test named |name| is scheduled to be started.
  void OnTestStarted(const std::string& name);

  // Called when all tests that were to be started have been scheduled
  // to be started.
  void OnAllTestsStarted();

  // Adds |result| to the stored test results.
  void AddTestResult(const TestResult& result);

  // Prints a summary of all test iterations (not just the last one) to stdout.
  void PrintSummaryOfAllIterations() const;

  // Saves a JSON summary of all test iterations results to |path|. Returns
  // true on success.
  bool SaveSummaryAsJSON(const FilePath& path) const WARN_UNUSED_RESULT;

 private:
  struct PerIterationData {
    PerIterationData();
    ~PerIterationData();

    // Test results grouped by test case name.
    typedef std::map<std::string, std::vector<TestResult> > ResultsMap;
    ResultsMap results;

    // List of full names of failed tests.
    typedef std::map<TestResult::Status, std::vector<std::string> > StatusMap;
    StatusMap tests_by_status;

    size_t test_started_count;

    // Total number of tests run.
    size_t test_run_count;

    TestsResultCallback callback;
  };

  // Prints a list of tests that finished with |status|.
  void PrintTestsByStatus(TestResult::Status status,
                          const std::string& description);

  ThreadChecker thread_checker_;

  // Store test results for each iteration.
  std::vector<PerIterationData> per_iteration_data_;

  // Index of current iteration (starting from 0). -1 before the first
  // iteration.
  int iteration_;

  // File handle of output file (can be NULL if no file).
  FILE* out_;

  DISALLOW_COPY_AND_ASSIGN(TestResultsTracker);
};

}  // namespace base

#endif  // BASE_TEST_LAUNCHER_TEST_RESULTS_TRACKER_H_
