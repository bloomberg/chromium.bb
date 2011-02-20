// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/webdriver/commands/implicit_wait_command.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/error_codes.h"
#include "chrome/test/webdriver/session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webdriver {

namespace {

void AssertIsAbleToInitialize(ImplicitWaitCommand* const command) {
  Response response;
  EXPECT_TRUE(command->Init(&response));
  ASSERT_EQ(kSuccess, response.GetStatus()) << response.ToJSON();
}

void AssertError(ErrorCode expected_status,
                 const std::string& expected_message,
                 ImplicitWaitCommand* const command) {
  Response response;
  command->ExecutePost(&response);
  ASSERT_EQ(expected_status,  response.GetStatus()) << response.ToJSON();

  const Value* value = response.GetValue();
  ASSERT_TRUE(value->IsType(Value::TYPE_DICTIONARY));

  const DictionaryValue* dict = static_cast<const DictionaryValue*>(value);
  std::string actual_message;
  ASSERT_TRUE(dict->GetString("message", &actual_message));
  ASSERT_EQ(expected_message, actual_message);
}

void AssertTimeoutSet(const Session& test_session, int expected_timeout,
                      ImplicitWaitCommand* const command) {
  Response response;
  command->ExecutePost(&response);
  ASSERT_EQ(kSuccess, response.GetStatus()) << response.ToJSON();
  ASSERT_EQ(expected_timeout, test_session.implicit_wait());
}

}  // namespace

TEST(ImplicitWaitCommandTest, SettingImplicitWaits) {
  Session test_session;
  ASSERT_EQ(0, test_session.implicit_wait()) << "Sanity check failed";

  std::vector<std::string> path_segments;
  path_segments.push_back("");
  path_segments.push_back("session");
  path_segments.push_back(test_session.id());
  path_segments.push_back("timeouts");
  path_segments.push_back("implicitly_wait");

  DictionaryValue* parameters = new DictionaryValue;  // Owned by |command|.
  ImplicitWaitCommand command(path_segments, parameters);

  AssertIsAbleToInitialize(&command);

  AssertError(kBadRequest, "Request missing ms parameter", &command);

  parameters->Set("ms", Value::CreateNullValue());
  AssertError(kBadRequest, "ms parameter is not a number", &command);

  parameters->SetString("ms", "apples");
  AssertError(kBadRequest, "ms parameter is not a number", &command);

  parameters->SetInteger("ms", -1);
  AssertError(kBadRequest, "Wait must be non-negative: -1", &command);

  parameters->SetDouble("ms", -3.0);
  AssertError(kBadRequest, "Wait must be non-negative: -3", &command);

  parameters->SetInteger("ms", 1);
  AssertTimeoutSet(test_session, 1, &command);
  parameters->SetInteger("ms", 2.5);
  AssertTimeoutSet(test_session, 2, &command);
  parameters->SetInteger("ms", 0);
  AssertTimeoutSet(test_session, 0, &command);
}

}  // namespace webdriver
