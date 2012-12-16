// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome_impl.h"
#include "chrome/test/chromedriver/devtools_client.h"
#include "chrome/test/chromedriver/status.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ParsePagesInfo, Normal) {
  std::list<std::string> urls;
  Status status = internal::ParsePagesInfo(
      "[{\"webSocketDebuggerUrl\": \"http://debugurl\"}]",
      &urls);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, urls.size());
  ASSERT_EQ("http://debugurl", urls.front());
}

TEST(ParsePagesInfo, Multiple) {
  std::list<std::string> urls;
  Status status = internal::ParsePagesInfo(
      "[{\"webSocketDebuggerUrl\": \"http://debugurl\"},"
      " {\"webSocketDebuggerUrl\": \"http://debugurl2\"}]",
      &urls);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ("http://debugurl", urls.front());
  ASSERT_EQ("http://debugurl2", urls.back());
}

namespace {

void AssertFails(const std::string& data) {
  std::list<std::string> urls;
  Status status = internal::ParsePagesInfo(data, &urls);
  ASSERT_FALSE(status.IsOk());
  ASSERT_EQ(0u, urls.size());
}

}  // namespace

TEST(ParsePagesInfo, InvalidJSON) {
  AssertFails("[");
}

TEST(ParsePagesInfo, NonList) {
  AssertFails("{}");
}

TEST(ParsePagesInfo, NonDictionary) {
  AssertFails("[1]");
}

TEST(ParsePagesInfo, NoDebuggerUrl) {
  AssertFails("[{\"hi\": 1}]");
}

TEST(ParsePagesInfo, InvalidDebuggerUrl) {
  AssertFails("[{\"webSocketDebuggerUrl\": 1}]");
}

namespace {

class FakeDevToolsClient : public DevToolsClient {
 public:
  FakeDevToolsClient() : status_(kOk) {}
  virtual ~FakeDevToolsClient() {}

  void set_status(const Status& status) {
    status_ = status;
  }
  void set_result(const base::DictionaryValue& result) {
    result_.Clear();
    result_.MergeDictionary(&result);
  }

  // Overridden from DevToolsClient:
  virtual Status SendCommand(const std::string& method,
                             const base::DictionaryValue& params) OVERRIDE {
    return SendCommandAndGetResult(method, params, NULL);
  }
  virtual Status SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      scoped_ptr<base::DictionaryValue>* result) OVERRIDE {
    if (status_.IsError())
      return status_;
    result->reset(result_.DeepCopy());
    return Status(kOk);
  }

 private:
  Status status_;
  base::DictionaryValue result_;
};

void AssertEvalFails(const base::DictionaryValue& command_result) {
  scoped_ptr<base::Value> result;
  FakeDevToolsClient client;
  client.set_result(command_result);
  Status status = internal::EvaluateScript(&client, "", &result);
  ASSERT_EQ(kUnknownError, status.code());
  ASSERT_FALSE(result);
}

}  // namespace

TEST(ChromeImplEvaluateScript, CommandError) {
  scoped_ptr<base::Value> result;
  FakeDevToolsClient client;
  client.set_status(Status(kUnknownError));
  Status status = internal::EvaluateScript(&client, "", &result);
  ASSERT_EQ(kUnknownError, status.code());
  ASSERT_FALSE(result);
}

TEST(ChromeImplEvaluateScript, MissingResult) {
  base::DictionaryValue dict;
  ASSERT_NO_FATAL_FAILURE(AssertEvalFails(dict));
}

TEST(ChromeImplEvaluateScript, Throws) {
  base::DictionaryValue dict;
  dict.SetBoolean("wasThrown", true);
  dict.SetString("result.type", "undefined");
  ASSERT_NO_FATAL_FAILURE(AssertEvalFails(dict));
}

TEST(ChromeImplEvaluateScript, MissingType) {
  base::DictionaryValue dict;
  dict.SetBoolean("wasThrown", false);
  dict.SetInteger("result.value", 1);
  ASSERT_NO_FATAL_FAILURE(AssertEvalFails(dict));
}

TEST(ChromeImplEvaluateScript, Undefined) {
  scoped_ptr<base::Value> result;
  FakeDevToolsClient client;
  base::DictionaryValue dict;
  dict.SetBoolean("wasThrown", false);
  dict.SetString("result.type", "undefined");
  client.set_result(dict);
  Status status = internal::EvaluateScript(&client, "", &result);
  ASSERT_EQ(kOk, status.code());
  ASSERT_TRUE(result && result->IsType(base::Value::TYPE_NULL));
}

TEST(ChromeImplEvaluateScript, Value) {
  scoped_ptr<base::Value> result;
  FakeDevToolsClient client;
  base::DictionaryValue dict;
  dict.SetBoolean("wasThrown", false);
  dict.SetString("result.type", "integer");
  dict.SetInteger("result.value.status", 0);
  dict.SetInteger("result.value.value", 1);
  client.set_result(dict);
  Status status = internal::EvaluateScript(&client, "", &result);
  ASSERT_EQ(kOk, status.code());
  int value;
  ASSERT_TRUE(result && result->GetAsInteger(&value));
  ASSERT_EQ(1, value);
}

TEST(ChromeImplEvaluateScript, ScriptError) {
  scoped_ptr<base::Value> result;
  FakeDevToolsClient client;
  base::DictionaryValue dict;
  dict.SetBoolean("wasThrown", false);
  dict.SetString("result.type", "integer");
  dict.SetInteger("result.value.status", 1);
  dict.SetInteger("result.value.value", 1);
  client.set_result(dict);
  Status status = internal::EvaluateScript(&client, "", &result);
  ASSERT_EQ(1, status.code());
  ASSERT_FALSE(result);
}
