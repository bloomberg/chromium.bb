// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_LAUNCHER_H_
#define BASE_TEST_TEST_LAUNCHER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace testing {
class TestCase;
class TestInfo;
}

namespace base {

// Constants for GTest command-line flags.
extern const char kGTestFilterFlag[];
extern const char kGTestListTestsFlag[];
extern const char kGTestRepeatFlag[];
extern const char kGTestRunDisabledTestsFlag[];
extern const char kGTestOutputFlag[];

// Interface for use with LaunchTests that abstracts away exact details
// which tests and how are run.
class TestLauncherDelegate {
 public:
  // Called before a test is considered for running. If it returns false,
  // the test is not run. If it returns true, the test will be run provided
  // it is part of the current shard.
  virtual bool ShouldRunTest(const testing::TestCase* test_case,
                             const testing::TestInfo* test_info) = 0;

  // Called to make the delegate run specified test. Should return true
  // on success.
  virtual bool RunTest(const testing::TestCase* test_case,
                       const testing::TestInfo* test_info) = 0;

 protected:
  virtual ~TestLauncherDelegate();
};

// Launches GTest-based tests from the current executable
// using |launcher_delegate|.
int LaunchTests(TestLauncherDelegate* launcher_delegate,
                int argc,
                char** argv) WARN_UNUSED_RESULT;

}  // namespace base

#endif  // BASE_TEST_TEST_LAUNCHER_H_
