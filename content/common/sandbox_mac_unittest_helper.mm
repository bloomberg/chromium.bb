// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_mac_unittest_helper.h"

extern "C" {
#include <sandbox.h>
}

#include <map>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "content/common/sandbox_mac.h"
#include "testing/multiprocess_func_list.h"

using sandbox::Sandbox;

namespace {

const char* kSandboxTypeKey = "CHROMIUM_SANDBOX_SANDBOX_TYPE";
const char* kSandboxTestNameKey = "CHROMIUM_SANDBOX_TEST_NAME";
const char* kTestDataKey = "CHROMIUM_SANDBOX_USER_DATA";

}  // namespace

namespace sandboxtest {

// Support infrastructure for REGISTER_SANDBOX_TEST_CASE macro.
namespace internal {

typedef std::map<std::string,MacSandboxTestCase*> SandboxTestMap;

// A function that returns a common map from string -> test case class.
SandboxTestMap& GetSandboxTestMap() {
  static SandboxTestMap test_map;
  return test_map;
}

void AddSandboxTestCase(const char* test_name, MacSandboxTestCase* test_class) {
  SandboxTestMap& test_map = GetSandboxTestMap();
  if (test_map.find(test_name) != test_map.end()) {
    LOG(ERROR) << "Trying to register duplicate test" << test_name;
    NOTREACHED();
  }
  test_map[test_name] = test_class;
}

}  // namespace internal

bool MacSandboxTest::RunTestInAllSandboxTypes(const char* test_name,
                                              const char* test_data) {
  // Go through all the sandbox types, and run the test case in each of them
  // if one fails, abort.
  for(int i = static_cast<int>(Sandbox::SANDBOX_TYPE_FIRST_TYPE);
      i < Sandbox::SANDBOX_AFTER_TYPE_LAST_TYPE;
      ++i) {

    if (!RunTestInSandbox(static_cast<Sandbox::SandboxProcessType>(i),
            test_name, test_data)) {
      LOG(ERROR) << "Sandboxed test (" << test_name << ")" <<
          "Failed in sandbox type " << i <<
          "user data: (" << test_data << ")";
      return false;
    }
  }
 return true;
}

bool MacSandboxTest::RunTestInSandbox(Sandbox::SandboxProcessType sandbox_type,
                                      const char* test_name,
                                      const char* test_data) {
  std::stringstream s;
  s << static_cast<int>(static_cast<int>(sandbox_type));
  setenv(kSandboxTypeKey, s.str().c_str(), 1);
  setenv(kSandboxTestNameKey, test_name, 1);
  if (test_data)
    setenv(kTestDataKey, test_data, 1);

  base::ProcessHandle child_process = SpawnChild("mac_sandbox_test_runner",
                                                 false);
  int code = -1;
  if (!base::WaitForExitCode(child_process, &code)) {
    LOG(WARNING) << "base::WaitForExitCode failed";
    return false;
  }
  return code == 0;
}

bool MacSandboxTestCase::BeforeSandboxInit() {
  return true;
}

void MacSandboxTestCase::SetTestData(const char* test_data) {
  test_data_ = test_data;
}

// Given a test name specified by |name| return that test case.
// If no test case is found for the given name, return NULL.
MacSandboxTestCase *SandboxTestForName(const char* name) {
  using internal::SandboxTestMap;
  using internal::GetSandboxTestMap;

  SandboxTestMap all_tests = GetSandboxTestMap();

  SandboxTestMap::iterator it = all_tests.find(name);
  if (it == all_tests.end()) {
    LOG(ERROR) << "Couldn't find sandbox test case(" << name << ")";
    return NULL;
  }

  return it->second;
}

}  // namespace sandboxtest

namespace {

// Main function for driver process that enables the sandbox and runs test
// code.
MULTIPROCESS_TEST_MAIN(mac_sandbox_test_runner) {
  // Extract parameters.
  char* sandbox_type_str = getenv(kSandboxTypeKey);
  if (!sandbox_type_str) {
    LOG(ERROR) << "Sandbox type not specified";
    return -1;
  }
  Sandbox::SandboxProcessType sandbox_type =
      static_cast<Sandbox::SandboxProcessType>(atoi(sandbox_type_str));
  char* sandbox_test_name = getenv(kSandboxTestNameKey);
  if (!sandbox_test_name) {
    LOG(ERROR) << "Sandbox test name not specified";
    return -1;
  }

  const char* test_data = getenv(kTestDataKey);

  // Find Test Function to run;
  scoped_ptr<sandboxtest::MacSandboxTestCase>
      test_case(sandboxtest::SandboxTestForName(sandbox_test_name));
  if (!test_case.get()) {
    LOG(ERROR) << "Invalid sandbox test name (" << sandbox_test_name << ")";
    return -1;
  }
  test_case->SetTestData(test_data);

  // Run Test.
  if (!test_case->BeforeSandboxInit()) {
    LOG(ERROR) << sandbox_test_name << "Failed test before sandbox init";
    return -1;
  }

  Sandbox::SandboxWarmup(sandbox_type);

  if (!Sandbox::EnableSandbox(sandbox_type, FilePath())) {
    LOG(ERROR) << "Failed to initialize sandbox " << sandbox_type;
    return -1;
  }

  if (!test_case->SandboxedTest()) {
    LOG(ERROR) << sandbox_test_name << "Failed sandboxed test";
    return -1;
  }

  return 0;
}

}  // namespace
