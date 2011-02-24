// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SANDBOX_MAC_UNITTEST_HELPER_H_
#define CHROME_COMMON_SANDBOX_MAC_UNITTEST_HELPER_H_
#pragma once

#include "base/test/multiprocess_test.h"
#include "chrome/common/sandbox_mac.h"

namespace sandboxtest {

// Helpers for writing unit tests that runs in the context of the Mac sandbox.
//
// How to write a sandboxed test:
// 1. Create a class that inherits from MacSandboxTestCase and overrides
// its functions to run code before or after the sandbox is initialised in a
// subprocess.
// 2. Register the class you just created with the REGISTER_SANDBOX_TEST_CASE()
// macro.
// 3. Write a test [using TEST_F()] that inherits from MacSandboxTest and call
// one of its helper functions to launch the test.
//
// Example:
//  class TestCaseThatRunsInSandboxedSubprocess :
//      public sandboxtest::MacSandboxTestCase {
//   public:
//    virtual bool SandboxedTest() {
//      .. test code that runs in sandbox goes here ..
//      return true; // always succeed.
//    }
//  };
//
//  // Register the test case you just created.
//  REGISTER_SANDBOX_TEST_CASE(TestCaseThatRunsInSandboxedSubprocess);
//
//  TEST_F(MacSandboxTest, ATest) {
//    EXPECT_TRUE(RunTestInAllSandboxTypes(
//                    "TestCaseThatRunsInSandboxedSubprocess",
//                    NULL));
//  }

// Base test type with helper functions to spawn a subprocess that exercises
// a given test in the sandbox.
class MacSandboxTest : public base::MultiProcessTest {
 public:
  // Runs a test specified by |test_name| in a sandbox of the type specified
  // by |sandbox_type|. |test_data| is a custom string that a test can pass
  // to the child process runing in the sandbox.
  // Returns true if the test passes, false if either of the functions in
  // the corresponding MacSandboxTestCase return false.
  bool RunTestInSandbox(sandbox::Sandbox::SandboxProcessType sandbox_type,
                        const char* test_name,
                        const char* test_data);

  // Runs the test specified by |test_name| in all the different sandbox types,
  // one by one.
  // Returns true if the test passes, false if either of the functions in
  // the corresponding MacSandboxTestCase return false in any of the spawned
  // processes.
  bool RunTestInAllSandboxTypes(const char* test_name,
                                const char* test_data);
};

// Class to ease writing test cases that run inside the OS X sandbox.
// This class is instantiated in a subprocess, and allows you to run test code
// at various stages of execution.
// Note that you must register the subclass you create with the
// REGISTER_SANDBOX_TEST_CASE so it's visible to the test driver.
class MacSandboxTestCase {
 public:
  virtual ~MacSandboxTestCase() {}

  // Code that runs in the sandboxed subprocess before the sandbox is
  // initialized.
  // Returning false from this function will cause the entire test case to fail.
  virtual bool BeforeSandboxInit();

  // Code that runs in the sandboxed subprocess when the sandbox has been
  // enabled.
  // Returning false from this function will cause the entire test case to fail.
  virtual bool SandboxedTest() = 0;

  // The data that's passed in the |user_data| parameter of
  // RunTest[s]InSandbox() is passed to this function.
  virtual void SetTestData(const char* test_data);

 protected:
  std::string test_data_;
};

// Plumbing to support the REGISTER_SANDBOX_TEST_CASE macro.
namespace internal {

// Register a test case with a given name.
void AddSandboxTestCase(const char* test_name, MacSandboxTestCase* test_class);

// Construction of this class causes a new entry to be placed in a global
// map.
template <class T> struct RegisterSandboxTest {
  RegisterSandboxTest(const char* test_name) {
    AddSandboxTestCase(test_name, new T);
  }
};

#define REGISTER_SANDBOX_TEST_CASE(class_name) \
  namespace { \
    sandboxtest::internal::RegisterSandboxTest<class_name> \
      register_test##class_name(#class_name); \
  }  // namespace

}  // namespace internal

}  // namespace sandboxtest

#endif  // CHROME_COMMON_SANDBOX_MAC_UNITTEST_HELPER_H_
