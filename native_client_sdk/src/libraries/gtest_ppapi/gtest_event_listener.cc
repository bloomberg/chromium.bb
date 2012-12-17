// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "gtest_ppapi/gtest_event_listener.h"

#include "ppapi/c/pp_macros.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

#if defined(WIN32)
#undef PostMessage
#endif

GTestEventListener::GTestEventListener(pp::Instance* instance)
  : instance_(instance),
    PP_ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {
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
  MyPostMessage(msg.str());
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
    if (test_part_result.file_name())
      msg << test_part_result.file_name();
    else
      msg << "<unknown filename>";
    msg << "::" << test_part_result.line_number() << "::";
    msg << test_part_result.summary();
    MyPostMessage(msg.str());
  }
}

void GTestEventListener::OnTestEnd(const ::testing::TestInfo& test_info) {
  std::stringstream msg;
  msg << "::test_end::";
  msg << test_info.test_case_name() << "." << test_info.name();

  msg << (test_info.result()->Failed() ? ": FAILED" : ": OK");
  MyPostMessage(msg.str());
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
  MyPostMessage(msg.str());
}

void GTestEventListener::MyPostMessage(const std::string& str) {
  if (pp::Module::Get()->core()->IsMainThread()) {
    instance_->PostMessage(str);
  } else {
    pp::CompletionCallback cc = factory_.NewCallback(
        &GTestEventListener::MyPostMessageCallback, str);
    pp::Module::Get()->core()->CallOnMainThread(0, cc, PP_OK);
  }
}

void GTestEventListener::MyPostMessageCallback(int32_t result,
                                             const std::string& str) {
  instance_->PostMessage(str);
}
