// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_LAUNCHER_TEST_RESULT_H_
#define BASE_TEST_LAUNCHER_TEST_RESULT_H_

#include <string>

#include "base/time/time.h"

namespace base {

// Structure containing result of a single test.
struct TestResult {
  enum Status {
    TEST_UNKNOWN,  // Status not set.
    TEST_SUCCESS,  // Test passed.
    TEST_FAILURE,  // Assertion failure (think EXPECT_TRUE, not DCHECK).
    TEST_TIMEOUT,  // Test timed out and was killed.
    TEST_CRASH,    // Test crashed (includes CHECK/DCHECK failures).
    TEST_SKIPPED,  // Test skipped (not run at all).
  };

  TestResult();
  ~TestResult();

  std::string GetFullName() const { return test_case_name + "." + test_name; }

  // Returns the test status as string (e.g. for display).
  std::string StatusAsString() const;

  // Returns true if the test has completed (i.e. the test binary exited
  // normally, possibly with an exit code indicating failure, but didn't crash
  // or time out in the middle of the test).
  bool completed() const {
    return status == TEST_SUCCESS || status == TEST_FAILURE;
  }

  // Name of the test case (before the dot, e.g. "A" for test "A.B").
  std::string test_case_name;

  // Name of the test (after the dot, e.g. "B" for test "A.B").
  std::string test_name;

  Status status;

  // Time it took to run the test.
  base::TimeDelta elapsed_time;

  // Output of just this test (optional).
  std::string output_snippet;
};

}  // namespace base

#endif  // BASE_TEST_LAUNCHER_TEST_RESULT_H_
