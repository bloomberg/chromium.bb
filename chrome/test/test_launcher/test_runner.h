// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TEST_LAUNCHER_TEST_RUNNER_H_
#define CHROME_TEST_TEST_LAUNCHER_TEST_RUNNER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace tests {

class TestRunnerFactory;

// Runs the tests specified by the --gtest_filter flag specified in the command
// line that started this process.
// Returns true if all tests succeeded, false if there were no tests to run, or
// one or more tests failed, or if initialization failed.
// Results are printed to stdout.
bool RunTests(const TestRunnerFactory& test_runner_factory);

// This class defines a way to run tests in an isolated environment (each test
// having its static variables uninitialized).
// There are 2 implementations, in-process and out-of-process.
class TestRunner {
 public:
  TestRunner();
  virtual ~TestRunner();

  // Called once before the TestRunner is used.  Gives it an opportunity to
  // perform any requried initialization.  Should return true if the
  // initialization was successful.
  virtual bool Init() = 0;

  // Runs the test named |test_name| and returns true if the test succeeded,
  // false if it failed.
  virtual bool RunTest(const std::string& test_name) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestRunner);
};

class TestRunnerFactory {
 public:
  virtual TestRunner* CreateTestRunner() const = 0;
};

}  // namespace

#endif  // CHROME_TEST_TEST_LAUNCHER_TEST_RUNNER_H_
