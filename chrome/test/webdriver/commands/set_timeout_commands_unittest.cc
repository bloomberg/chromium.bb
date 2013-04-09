// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/commands/set_timeout_commands.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_session.h"
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

  const base::Value* value = response.GetValue();
  ASSERT_TRUE(value->IsType(base::Value::TYPE_DICTIONARY));

  const base::DictionaryValue* dict =
      static_cast<const base::DictionaryValue*>(value);
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
  path_segments.push_back(std::string());
  path_segments.push_back("session");
  path_segments.push_back(test_session.id());
  path_segments.push_back("timeouts");
  path_segments.push_back("implicitly_wait");

  base::DictionaryValue* parameters =
      new base::DictionaryValue;  // Owned by |command|.
  ImplicitWaitCommand command(path_segments, parameters);

  AssertIsAbleToInitialize(&command);

  AssertError(kBadRequest, "Request missing ms parameter", &command);

  parameters->Set("ms", base::Value::CreateNullValue());
  AssertError(kBadRequest, "ms parameter is not a number", &command);

  parameters->SetString("ms", "apples");
  AssertError(kBadRequest, "ms parameter is not a number", &command);

  parameters->SetInteger("ms", -1);
  AssertError(kBadRequest, "Timeout must be non-negative", &command);

  parameters->SetDouble("ms", -3.0);
  AssertError(kBadRequest, "Timeout must be non-negative", &command);

  parameters->SetInteger("ms", 1);
  AssertTimeoutSet(test_session, 1, &command);
  parameters->SetDouble("ms", 2.5);
  AssertTimeoutSet(test_session, 2, &command);
  parameters->SetInteger("ms", 0);
  AssertTimeoutSet(test_session, 0, &command);
}

}  // namespace webdriver
