// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_logger.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeDevToolsClient : public StubDevToolsClient {
 public:
  explicit FakeDevToolsClient(const std::string& id) : id_(id) {}
  virtual ~FakeDevToolsClient() {}

  std::string PopSentCommand() {
    std::string command;
    if (!sent_command_queue_.empty()) {
      command = sent_command_queue_.front();
      sent_command_queue_.pop_front();
    }
    return command;
  }

  void TriggerEvent(const std::string& method) {
    base::DictionaryValue empty_params;
    listener_->OnEvent(this, method, empty_params);
  }

  // Overridden from DevToolsClient:
  virtual Status ConnectIfNecessary() OVERRIDE {
    return listener_->OnConnected(this);
  }

  virtual Status SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      scoped_ptr<base::DictionaryValue>* result) OVERRIDE {
    sent_command_queue_.push_back(method);
    return Status(kOk);
  }

  virtual void AddListener(DevToolsEventListener* listener) OVERRIDE {
    listener_ = listener;
  }

  virtual const std::string& GetId() OVERRIDE {
    return id_;
  }

 private:
  const std::string id_;
  std::list<std::string> sent_command_queue_;
  DevToolsEventListener* listener_;
};

scoped_ptr<DictionaryValue> ParseDictionary(const std::string& json) {
  std::string error;
  scoped_ptr<Value> value(base::JSONReader::ReadAndReturnError(
      json, base::JSON_PARSE_RFC, NULL, &error));
  if (NULL == value) {
    SCOPED_TRACE(json.c_str());
    SCOPED_TRACE(error.c_str());
    ADD_FAILURE();
    return scoped_ptr<DictionaryValue>(NULL);
  }
  DictionaryValue* dict = 0;
  if (!value->GetAsDictionary(&dict)) {
    SCOPED_TRACE("JSON object is not a dictionary");
    ADD_FAILURE();
    return scoped_ptr<DictionaryValue>(NULL);
  }
  return scoped_ptr<DictionaryValue>(dict->DeepCopy());
}

void ValidateLogEntry(base::ListValue *entries,
                      int index,
                      const char* expect_webview,
                      const char* expect_method,
                      const char* expect_level) {
  const base::DictionaryValue *entry;
  ASSERT_TRUE(entries->GetDictionary(index, &entry));
  std::string message_json;
  ASSERT_TRUE(entry->GetString("message", &message_json));
  scoped_ptr<base::DictionaryValue> message(ParseDictionary(message_json));

  std::string level;
  EXPECT_TRUE(entry->GetString("level", &level));
  EXPECT_STREQ(expect_level, level.c_str());
  double timestamp = 0;
  EXPECT_TRUE(entry->GetDouble("timestamp", &timestamp));
  EXPECT_LT(0, timestamp);
  std::string webview;
  EXPECT_TRUE(message->GetString("webview", &webview));
  EXPECT_STREQ(expect_webview, webview.c_str());
  std::string method;
  EXPECT_TRUE(message->GetString("message.method", &method));
  EXPECT_STREQ(expect_method, method.c_str());
  DictionaryValue* params;
  EXPECT_TRUE(message->GetDictionary("message.params", &params));
  EXPECT_EQ(0u, params->size());
}

}  // namespace

TEST(DevToolsEventLogger, OneClientMultiDomains) {
  FakeDevToolsClient client("webview-1");
  std::vector<std::string> domains;
  domains.push_back("Page");
  domains.push_back("Network");
  domains.push_back("Timeline");
  DevToolsEventLogger logger("mylog", domains, "INFO");

  client.AddListener(&logger);
  logger.OnConnected(&client);
  EXPECT_STREQ("Page.enable", client.PopSentCommand().c_str());
  EXPECT_STREQ("Network.enable", client.PopSentCommand().c_str());
  EXPECT_STREQ("Timeline.start", client.PopSentCommand().c_str());
  EXPECT_STREQ("", client.PopSentCommand().c_str());
  client.TriggerEvent("Network.gaga");
  client.TriggerEvent("Page.ulala");
  client.TriggerEvent("Console.bad");  // Ignore -- different domain.

  scoped_ptr<base::ListValue> entries(logger.GetAndClearLogEntries());

  ASSERT_EQ(2u, entries->GetSize());
  ValidateLogEntry(entries.get(), 0, "webview-1", "Network.gaga", "INFO");
  ValidateLogEntry(entries.get(), 1, "webview-1", "Page.ulala", "INFO");

  // Repeat get returns nothing.
  scoped_ptr<base::ListValue> no_entries(logger.GetAndClearLogEntries());
  EXPECT_EQ(0u, no_entries->GetSize());

  EXPECT_STREQ("", client.PopSentCommand().c_str());  // No more commands sent.
}

TEST(DevToolsEventLogger, MultiClientsOneDomain) {
  FakeDevToolsClient client1("webview-1");
  FakeDevToolsClient client2("webview-2");
  std::vector<std::string> domains;
  domains.push_back("Console");
  DevToolsEventLogger logger("mylog", domains, "INFO");

  client1.AddListener(&logger);
  client2.AddListener(&logger);
  logger.OnConnected(&client1);
  logger.OnConnected(&client2);
  EXPECT_STREQ("Console.enable", client1.PopSentCommand().c_str());
  EXPECT_STREQ("", client1.PopSentCommand().c_str());
  EXPECT_STREQ("Console.enable", client2.PopSentCommand().c_str());
  EXPECT_STREQ("", client2.PopSentCommand().c_str());
  // OnConnected sends the enable command only to that client, not others.
  client1.ConnectIfNecessary();
  EXPECT_STREQ("Console.enable", client1.PopSentCommand().c_str());
  EXPECT_STREQ("", client1.PopSentCommand().c_str());
  EXPECT_STREQ("", client2.PopSentCommand().c_str());

  client1.TriggerEvent("Console.gaga1");
  client2.TriggerEvent("Console.gaga2");

  scoped_ptr<base::ListValue> entries(logger.GetAndClearLogEntries());

  ASSERT_EQ(2u, entries->GetSize());
  ValidateLogEntry(entries.get(), 0, "webview-1", "Console.gaga1", "INFO");
  ValidateLogEntry(entries.get(), 1, "webview-2", "Console.gaga2", "INFO");
}
