// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef GTEST_PPAPI_GTEST_EVENT_LISTENER_H_
#define GTEST_PPAPI_GTEST_EVENT_LISTENER_H_

#include <string>

#include "gtest/gtest.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace pp {
class Instance;
}  // namespace pp

// GTestEventListener is a gtest event listener that performs two functions:
// 1. It redirects output to PostMessage rather than printf, so that test events
//    are dispatched to JS.
// 2. Channels post-message dispatches to the main thread via callbacks, so that
//    it can be used from any thread.
//
// GTestEventListener formats gtest event messages to be parsed by javascript
// helper functions found in gtest_runner.js
class GTestEventListener : public ::testing::EmptyTestEventListener {
 public:
  explicit GTestEventListener(pp::Instance* instance);

  // TestEventListener overrides.
  virtual void OnTestProgramStart(const ::testing::UnitTest& unit_test);
  virtual void OnTestCaseStart(const ::testing::TestCase& test_case);
  virtual void OnTestStart(const ::testing::TestInfo& test_info);
  virtual void OnTestPartResult(
      const ::testing::TestPartResult& test_part_result);
  virtual void OnTestEnd(const ::testing::TestInfo& test_info);
  virtual void OnTestCaseEnd(const ::testing::TestCase& test_case);
  virtual void OnTestProgramEnd(const ::testing::UnitTest& unit_test);

 private:
  // Called MyPostMessage as to not conflict with win32 macro PostMessage.
  void MyPostMessage(const std::string& str);
  void MyPostMessageCallback(int32_t result, const std::string& str);

  pp::Instance* instance_;
  pp::CompletionCallbackFactory<GTestEventListener> factory_;
};

#endif  // GTEST_PPAPI_GTEST_EVENT_LISTENER_H_
