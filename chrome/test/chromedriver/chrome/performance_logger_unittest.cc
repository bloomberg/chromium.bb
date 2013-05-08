// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/performance_logger.h"

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_vector.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
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

struct LogEntry {
  const base::Time timestamp;
  const Log::Level level;
  const std::string message;

  LogEntry(const base::Time& timestamp,
           Log::Level level,
           const std::string& message)
      : timestamp(timestamp), level(level), message(message) {}
};

class FakeLog : public Log {
 public:
  virtual void AddEntry(const base::Time& time,
                        Level level,
                        const std::string& message) OVERRIDE;

  ScopedVector<LogEntry> entries;
};

void FakeLog::AddEntry(
    const base::Time& time, Level level, const std::string& message) {
  entries.push_back(new LogEntry(time, level, message));
}

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

void ValidateLogEntry(LogEntry *entry,
                      const char* expect_webview,
                      const char* expect_method) {
  EXPECT_EQ(Log::kLog, entry->level);
  EXPECT_LT(0, entry->timestamp.ToTimeT());

  scoped_ptr<base::DictionaryValue> message(ParseDictionary(entry->message));
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

void ExpectEnableDomains(FakeDevToolsClient& client) {
  EXPECT_STREQ("Network.enable", client.PopSentCommand().c_str());
  EXPECT_STREQ("Page.enable", client.PopSentCommand().c_str());
  EXPECT_STREQ("Timeline.start", client.PopSentCommand().c_str());
  EXPECT_STREQ("", client.PopSentCommand().c_str());
}

}  // namespace

TEST(PerformanceLogger, OneWebView) {
  FakeDevToolsClient client("webview-1");
  FakeLog log;
  PerformanceLogger logger(&log);

  client.AddListener(&logger);
  logger.OnConnected(&client);
  ExpectEnableDomains(client);
  client.TriggerEvent("Network.gaga");
  client.TriggerEvent("Page.ulala");
  client.TriggerEvent("Console.bad");  // Ignore -- different domain.

  ASSERT_EQ(2u, log.entries.size());
  ValidateLogEntry(log.entries[0], "webview-1", "Network.gaga");
  ValidateLogEntry(log.entries[1], "webview-1", "Page.ulala");
}

TEST(PerformanceLogger, TwoWebViews) {
  FakeDevToolsClient client1("webview-1");
  FakeDevToolsClient client2("webview-2");
  FakeLog log;
  PerformanceLogger logger(&log);

  client1.AddListener(&logger);
  client2.AddListener(&logger);
  logger.OnConnected(&client1);
  logger.OnConnected(&client2);
  ExpectEnableDomains(client1);
  ExpectEnableDomains(client2);
  // OnConnected sends the enable command only to that client, not others.
  client1.ConnectIfNecessary();
  ExpectEnableDomains(client1);
  EXPECT_STREQ("", client2.PopSentCommand().c_str());

  client1.TriggerEvent("Page.gaga1");
  client2.TriggerEvent("Timeline.gaga2");

  ASSERT_EQ(2u, log.entries.size());
  ValidateLogEntry(log.entries[0], "webview-1", "Page.gaga1");
  ValidateLogEntry(log.entries[1], "webview-2", "Timeline.gaga2");
}
