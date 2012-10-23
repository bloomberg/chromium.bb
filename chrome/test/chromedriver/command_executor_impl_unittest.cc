// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/command_executor_impl.h"
#include "chrome/test/chromedriver/status.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(CommandExecutorTest, NewSession) {
  CommandExecutorImpl executor;
  base::DictionaryValue empty_dict;
  StatusCode status;
  scoped_ptr<base::Value> value;
  std::string session_id;
  executor.ExecuteCommand("newSession", empty_dict, "",
                          &status, &value, &session_id);
  ASSERT_EQ(kOk, status);
  ASSERT_TRUE(value.get());
  ASSERT_TRUE(value->IsType(base::Value::TYPE_NULL));
  ASSERT_STREQ("1", session_id.c_str());
  executor.ExecuteCommand("newSession", empty_dict, "random",
                          &status, &value, &session_id);
  ASSERT_STREQ("2", session_id.c_str());
}

TEST(CommandExecutorTest, UnknownCommand) {
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
  ASSERT_STREQ("unknown command: noSuchCommand", error_msg.c_str());
  ASSERT_STREQ("session", session_id.c_str());
}

TEST(CommandExecutorTest, Quit) {
  CommandExecutorImpl executor;
  base::DictionaryValue empty_dict;
  StatusCode status;
  scoped_ptr<base::Value> value;
  std::string session_id;
  executor.ExecuteCommand("quit", empty_dict, "id",
                          &status, &value, &session_id);
  ASSERT_EQ(kOk, status);
  ASSERT_TRUE(value.get());
  ASSERT_TRUE(value->IsType(base::Value::TYPE_NULL));
  ASSERT_STREQ("id", session_id.c_str());
}
