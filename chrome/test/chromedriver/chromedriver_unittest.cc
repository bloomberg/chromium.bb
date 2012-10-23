// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chromedriver.h"
#include "chrome/test/chromedriver/command_executor.h"
#include "chrome/test/chromedriver/status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void ExpectExecuteError(const std::string& command) {
  std::string response;
  ExecuteCommand(command, &response);
  scoped_ptr<base::Value> value(base::JSONReader::Read(response));
  ASSERT_TRUE(value.get());
  base::DictionaryValue* dict;
  ASSERT_TRUE(value->GetAsDictionary(&dict));
  int status;
  ASSERT_TRUE(dict->GetInteger("status", &status));
  EXPECT_EQ(kUnknownError, status);
}

class ExecutorMock : public CommandExecutor {
 public:
  virtual ~ExecutorMock() {}

  virtual void ExecuteCommand(const std::string& name,
                              const base::DictionaryValue& params,
                              const std::string& session_id,
                              StatusCode* status,
                              scoped_ptr<base::Value>* value,
                              std::string* out_session_id) OVERRIDE {
    EXPECT_STREQ("name", name.c_str());
    int param = 0;
    EXPECT_TRUE(params.GetInteger("param", &param));
    EXPECT_EQ(100, param);
    EXPECT_STREQ("id", session_id.c_str());
    *status = kOk;
    value->reset(new base::StringValue("stuff"));
    *out_session_id = "session_id";
  }

  static void CheckExecuteCommand()  {
    std::string response;
    ::ExecuteCommand("{\"name\": \"name\", "
                     " \"parameters\": {\"param\": 100}, "
                     " \"sessionId\": \"id\"}",
                     &response);
    scoped_ptr<base::Value> value(base::JSONReader::Read(response));
    ASSERT_TRUE(value.get());
    base::DictionaryValue* dict;
    ASSERT_TRUE(value->GetAsDictionary(&dict));
    int status;
    ASSERT_TRUE(dict->GetInteger("status", &status));
    std::string value_str;
    ASSERT_TRUE(dict->GetString("value", &value_str));
    EXPECT_STREQ("stuff", value_str.c_str());
    EXPECT_EQ(kOk, status);
  }
};

CommandExecutor* CreateExecutorMock() {
  return new ExecutorMock();
}

}  // namespace

TEST(ChromeDriver, InvalidCommands) {
  ExpectExecuteError("hi[]");
  ExpectExecuteError("[]");
  ExpectExecuteError(
      "{\"parameters\": {}, \"sessionId\": \"\"}");
  ExpectExecuteError(
      "{\"name\": 1, \"parameters\": {}, \"sessionId\": \"\"}");
  ExpectExecuteError(
      "{\"name\": \"\", \"sessionId\": \"\"}");
  ExpectExecuteError(
      "{\"name\": \"\", \"parameters\": 1, \"sessionId\": \"\"}");
  ExpectExecuteError(
      "{\"name\": \"\", \"parameters\": {}}");
  ExpectExecuteError(
      "{\"name\": \"\", \"parameters\": {}, \"sessionId\": 1}");
}

TEST(ChromeDriver, ExecuteCommand) {
  SetCommandExecutorFactoryForTesting(&CreateExecutorMock);
  ExecutorMock::CheckExecuteCommand();
}
