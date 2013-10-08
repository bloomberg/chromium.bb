// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_LAUNCHER_TEST_RESULTS_TRACKER_H_
#define BASE_TEST_LAUNCHER_TEST_RESULTS_TRACKER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/launcher/test_launcher.h"
#include "base/threading/thread_checker.h"

class CommandLine;

namespace base {

// A helper class to output results.
// Note: as currently XML is the only supported format by gtest, we don't
// check output format (e.g. "xml:" prefix) here and output an XML file
// unconditionally.
// Note: we don't output per-test-case or total summary info like
// total failed_test_count, disabled_test_count, elapsed_time and so on.
// Only each test (testcase element in the XML) will have the correct
// failed/disabled/elapsed_time information. Each test won't include
// detailed failure messages either.
// TODO(phajdan.jr): Rename to TestResultsTracker.
class ResultsPrinter {
 public:
  typedef Callback<void(bool)> TestsResultCallback;

  ResultsPrinter(const CommandLine& command_line,
                 const TestsResultCallback& callback);

  // Called when test named |name| is scheduled to be started.
  void OnTestStarted(const std::string& name);

  // Called when all tests that were to be started have been scheduled
  // to be started.
  void OnAllTestsStarted();

  // Adds |result| to the stored test results.
  void AddTestResult(const TestResult& result);

  WeakPtr<ResultsPrinter> GetWeakPtr();

 private:
  ~ResultsPrinter();

  // Prints a list of tests that finished with |status|.
  void PrintTestsByStatus(TestResult::Status status,
                          const std::string& description);

  ThreadChecker thread_checker_;

  // Test results grouped by test case name.
  typedef std::map<std::string, std::vector<TestResult> > ResultsMap;
  ResultsMap results_;

  // List of full names of failed tests.
  typedef std::map<TestResult::Status, std::vector<std::string> > StatusMap;
  StatusMap tests_by_status_;

  size_t test_started_count_;

  // Total number of tests run.
  size_t test_run_count_;

  // File handle of output file (can be NULL if no file).
  FILE* out_;

  TestsResultCallback callback_;

  WeakPtrFactory<ResultsPrinter> weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(ResultsPrinter);
};

}  // namespace base

#endif  // BASE_TEST_LAUNCHER_TEST_RESULTS_TRACKER_H_
