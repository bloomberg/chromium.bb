// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_LAUNCHER_UNIT_TEST_LAUNCHER_H_
#define BASE_TEST_LAUNCHER_UNIT_TEST_LAUNCHER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/test/launcher/test_launcher.h"

namespace base {

// Callback that runs a test suite and returns exit code.
typedef base::Callback<int(void)> RunTestSuiteCallback;

// Launches unit tests in given test suite. Returns exit code.
int LaunchUnitTests(int argc,
                    char** argv,
                    const RunTestSuiteCallback& run_test_suite);

// Same as above, but always runs tests serially.
int LaunchUnitTestsSerially(int argc,
                            char** argv,
                            const RunTestSuiteCallback& run_test_suite);

#if defined(OS_WIN)
// Launches unit tests in given test suite. Returns exit code.
// |use_job_objects| determines whether to use job objects.
int LaunchUnitTests(int argc,
                    wchar_t** argv,
                    bool use_job_objects,
                    const RunTestSuiteCallback& run_test_suite);
#endif  // defined(OS_WIN)

// Test launcher delegate for unit tests (mostly to support batching).
class UnitTestLauncherDelegate : public TestLauncherDelegate {
 public:
  explicit UnitTestLauncherDelegate(size_t batch_limit, bool use_job_objects);
  ~UnitTestLauncherDelegate() override;

 private:
  struct GTestCallbackState {
    GTestCallbackState();
    ~GTestCallbackState();

    TestLauncher* test_launcher;
    std::vector<std::string> test_names;
    FilePath output_file;
  };

  // TestLauncherDelegate:
  bool ShouldRunTest(const std::string& test_case_name,
                     const std::string& test_name) override;
  size_t RunTests(TestLauncher* test_launcher,
                  const std::vector<std::string>& test_names) override;
  size_t RetryTests(TestLauncher* test_launcher,
                    const std::vector<std::string>& test_names) override;

  // Runs tests serially, each in its own process.
  void RunSerially(TestLauncher* test_launcher,
                   const std::vector<std::string>& test_names);

  // Runs tests in batches (each batch in its own process).
  void RunBatch(TestLauncher* test_launcher,
                const std::vector<std::string>& test_names);

  // Callback for batched tests.
  void GTestCallback(const GTestCallbackState& callback_state,
                     int exit_code,
                     const TimeDelta& elapsed_time,
                     bool was_timeout,
                     const std::string& output);

  // Callback for serialized tests.
  void SerialGTestCallback(const GTestCallbackState& callback_state,
                           const std::vector<std::string>& test_names,
                           int exit_code,
                           const TimeDelta& elapsed_time,
                           bool was_timeout,
                           const std::string& output);

  // Interprets test results and reports to the test launcher. Returns true
  // on success.
  static bool ProcessTestResults(TestLauncher* test_launcher,
                                 const std::vector<std::string>& test_names,
                                 const base::FilePath& output_file,
                                 const std::string& output,
                                 int exit_code,
                                 bool was_timeout,
                                 std::vector<std::string>* tests_to_relaunch);

  ThreadChecker thread_checker_;

  // Maximum number of tests to run in a single batch.
  size_t batch_limit_;

  // Determines whether we use job objects on Windows.
  bool use_job_objects_;

  DISALLOW_COPY_AND_ASSIGN(UnitTestLauncherDelegate);
};

}   // namespace base

#endif  // BASE_TEST_LAUNCHER_UNIT_TEST_LAUNCHER_H_
