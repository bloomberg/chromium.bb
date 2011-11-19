// Copyright 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "c_salt/test/gtest_event_listener.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

namespace c_salt {

GTestEventListener::GTestEventListener(pp::Instance* instance)
  : instance_(instance),
    factory_(this) {
  assert(pp::Module::Get()->core()->IsMainThread());
}

void GTestEventListener::OnTestProgramStart(
    const ::testing::UnitTest& unit_test) {
  std::stringstream msg;
  int num_tests = unit_test.test_to_run_count();
  int num_test_cases = unit_test.test_case_to_run_count();

  msg << "::start::" << num_tests << " test";
  if (num_tests > 1) msg << 's';
  msg << " from "<< num_test_cases << " test case";
  if (num_test_cases > 1) msg << 's';
  msg << '.';
  PostMessage(msg.str());
}

void GTestEventListener::OnTestCaseStart(
    const ::testing::TestCase& test_case) {
  // not currently used
}

void GTestEventListener::OnTestStart(const ::testing::TestInfo& test_info) {
  // not currently used
}

void GTestEventListener::OnTestPartResult(
      const ::testing::TestPartResult& test_part_result) {
  if (test_part_result.failed()) {
    std::stringstream msg;
    msg << "::test_failed::";
    msg << test_part_result.file_name();
    msg << "::" << test_part_result.line_number() << "::";
    msg << test_part_result.summary();
    PostMessage(msg.str());

    msg.str("");
    msg << "::failure_log::";
    msg << test_part_result.summary();
    PostMessage(msg.str());
  }
}

void GTestEventListener::OnTestEnd(const ::testing::TestInfo& test_info) {
  std::stringstream msg;
  msg << "::test_end::";
  msg << test_info.test_case_name() << "." << test_info.name();

  msg << (test_info.result()->Failed() ? ": FAILED" : ": OK");
  PostMessage(msg.str());
}

void GTestEventListener::OnTestCaseEnd(
    const ::testing::TestCase& test_case) {
  // not used
}

void GTestEventListener::OnTestProgramEnd(
    const ::testing::UnitTest& unit_test) {
  // Print info about what test and test cases ran.
  int num_passed_tests = unit_test.successful_test_count();
  int num_failed_tests = unit_test.failed_test_count();
  std::stringstream msg;
  msg << "::Result::";
  msg << ((num_failed_tests > 0) ? "failed::" : "success::");
  msg << num_passed_tests << "::" << num_failed_tests << "::";
  PostMessage(msg.str());
}

void GTestEventListener::PostMessage(const std::string& str) {
  if (pp::Module::Get()->core()->IsMainThread()) {
    instance_->PostMessage(str);
  } else {
    pp::CompletionCallback cc = factory_.NewCallback(
        &GTestEventListener::PostMessageCallback, str);
    pp::Module::Get()->core()->CallOnMainThread(0, cc, PP_OK);
  }
}

void GTestEventListener::PostMessageCallback(int32_t result,
                                             const std::string& str) {
  instance_->PostMessage(str);
}

}  // namespace c_salt

