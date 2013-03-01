// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/command_executor_impl.h"
#include "chrome/test/chromedriver/status.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(CommandExecutorImplTest, UnknownCommand) {
  CommandExecutorImpl executor;
  base::DictionaryValue empty_dict;
  StatusCode status;
  scoped_ptr<base::Value> value;
  std::string session_id;
  executor.ExecuteCommand("noSuchCommand", empty_dict, "session",
                          &status, &value, &session_id);
  ASSERT_EQ(kUnknownCommand, status);
  base::DictionaryValue* error;
  ASSERT_TRUE(value->GetAsDictionary(&error));
  std::string error_msg;
  ASSERT_TRUE(error->GetString("message", &error_msg));
  ASSERT_NE(std::string::npos, error_msg.find("noSuchCommand"));
  ASSERT_STREQ("session", session_id.c_str());
}

namespace {

Status ExecuteSimpleCommand(
    const base::DictionaryValue* expected_params,
    const std::string& expected_session_id,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* value,
    std::string* out_session_id) {
  EXPECT_EQ(expected_params, &params);
  EXPECT_STREQ(expected_session_id.c_str(), session_id.c_str());
  value->reset(new base::StringValue("hi"));
  *out_session_id = "out session id";
  return Status(kOk);
}

}  // namespace

TEST(CommandExecutorImplTest, SimpleCommand) {
  CommandExecutorImpl executor;
  base::DictionaryValue params;
  std::string session_id("some id");
  executor.command_map_.Set("simpleCommand",
      base::Bind(&ExecuteSimpleCommand, &params, session_id));

  StatusCode status_code;
  scoped_ptr<base::Value> value;
  std::string out_session_id;
  executor.ExecuteCommand("simpleCommand", params, session_id,
                          &status_code, &value,
                          &out_session_id);
  ASSERT_EQ(kOk, status_code);
  ASSERT_TRUE(value);
  base::StringValue hi("hi");
  ASSERT_TRUE(value->Equals(&hi));
  ASSERT_STREQ("out session id", out_session_id.c_str());
}

namespace {

Status ExecuteSimpleCommand2(
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id) {
  return Status(kOk);
}

}  // namespace

TEST(CommandExecutorImplTest, CommandThatDoesntSetValueOrSessionId) {
  CommandExecutorImpl executor;
  executor.command_map_.Set(
      "simpleCommand",
      base::Bind(&ExecuteSimpleCommand2));

  base::DictionaryValue params;
  StatusCode status_code;
  scoped_ptr<base::Value> value;
  std::string session_id;
  executor.ExecuteCommand("simpleCommand", params, "session",
                          &status_code, &value, &session_id);

  ASSERT_TRUE(value->IsType(base::Value::TYPE_NULL));
  ASSERT_STREQ("", session_id.c_str());
}

namespace {

Status ExecuteSimpleCommand3(
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* value,
    std::string* out_session_id) {
  value->reset(new base::StringValue("hi"));
  return Status(kUnknownError);
}

}  // namespace

TEST(CommandExecutorImplTest, CommandThatReturnsError) {
  CommandExecutorImpl executor;
  executor.command_map_.Set("simpleCommand",
                            base::Bind(&ExecuteSimpleCommand3));

  base::DictionaryValue params;
  StatusCode status_code;
  scoped_ptr<base::Value> value;
  std::string out_session_id;
  executor.ExecuteCommand("simpleCommand", params, "",
                          &status_code, &value,
                          &out_session_id);
  ASSERT_EQ(kUnknownError, status_code);
  ASSERT_TRUE(value);
  base::DictionaryValue* error;
  ASSERT_TRUE(value->GetAsDictionary(&error));
  std::string message;
  ASSERT_TRUE(error->GetString("message", &message));
  ASSERT_NE(std::string::npos, message.find("unknown error"));
}
