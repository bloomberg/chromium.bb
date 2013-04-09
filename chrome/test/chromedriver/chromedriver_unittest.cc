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
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chromedriver.h"
#include "chrome/test/chromedriver/command_executor.h"
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

class DummyExecutor : public CommandExecutor {
 public:
  virtual ~DummyExecutor() {}

  virtual void Init() OVERRIDE {}
  virtual void ExecuteCommand(const std::string& name,
                              const base::DictionaryValue& params,
                              const std::string& session_id,
                              StatusCode* status,
                              scoped_ptr<base::Value>* value,
                              std::string* out_session_id) OVERRIDE {}
};


struct ExpectedCommand {
  ExpectedCommand(
      const std::string& name,
      const base::DictionaryValue& in_params,
      const std::string& session_id,
      StatusCode return_status,
      scoped_ptr<base::Value> return_value,
      const std::string& return_session_id)
      : name(name),
        session_id(session_id),
        return_status(return_status),
        return_value(return_value.Pass()),
        return_session_id(return_session_id) {
    params.MergeDictionary(&in_params);
  }

  ~ExpectedCommand() {}

  std::string name;
  base::DictionaryValue params;
  std::string session_id;
  StatusCode return_status;
  scoped_ptr<base::Value> return_value;
  std::string return_session_id;
};

class ExecutorMock : public CommandExecutor {
 public:
  virtual ~ExecutorMock() {
    EXPECT_TRUE(DidSatisfyExpectations());
  }

  virtual void Init() OVERRIDE {}

  virtual void ExecuteCommand(const std::string& name,
                              const base::DictionaryValue& params,
                              const std::string& session_id,
                              StatusCode* status,
                              scoped_ptr<base::Value>* value,
                              std::string* out_session_id) OVERRIDE {
    ASSERT_TRUE(expectations_.size());
    ASSERT_STREQ(expectations_[0]->name.c_str(), name.c_str());
    ASSERT_TRUE(expectations_[0]->params.Equals(&params));
    ASSERT_STREQ(expectations_[0]->session_id.c_str(), session_id.c_str());
    *status = expectations_[0]->return_status;
    value->reset(expectations_[0]->return_value.release());
    *out_session_id = expectations_[0]->return_session_id;
    expectations_.erase(expectations_.begin());
  }

  void Expect(scoped_ptr<ExpectedCommand> expected) {
    expectations_.push_back(expected.release());
  }

  bool DidSatisfyExpectations() const {
    return expectations_.empty();
  }

 private:
  ScopedVector<ExpectedCommand> expectations_;
};

}  // namespace

TEST(ChromeDriver, InvalidCommands) {
  Init(scoped_ptr<CommandExecutor>(new DummyExecutor()));
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
  Shutdown();
}

TEST(ChromeDriver, ExecuteCommand) {
  scoped_ptr<ExecutorMock> scoped_mock(new ExecutorMock());
  ExecutorMock* mock = scoped_mock.get();
  Init(scoped_mock.PassAs<CommandExecutor>());
  {
    base::DictionaryValue params;
    params.SetInteger("param", 100);
    scoped_ptr<base::Value> value(new base::StringValue("stuff"));
    mock->Expect(scoped_ptr<ExpectedCommand>(new ExpectedCommand(
        "name", params, "id", kOk, value.Pass(), "session_id")));
  }
  std::string response;
  ExecuteCommand("{\"name\": \"name\", "
                 " \"parameters\": {\"param\": 100}, "
                 " \"sessionId\": \"id\"}",
                 &response);
  ASSERT_TRUE(mock->DidSatisfyExpectations());
  {
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
  {
    base::DictionaryValue params;
    scoped_ptr<base::Value> value(base::Value::CreateNullValue());
    mock->Expect(scoped_ptr<ExpectedCommand>(new ExpectedCommand(
        "quitAll", params, std::string(), kOk, value.Pass(), std::string())));
  }
  Shutdown();
}
