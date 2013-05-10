// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/console_logger.h"

#include "base/compiler_specific.h"
#include "base/format_macros.h"
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

  void TriggerEvent(const std::string& method,
                    const base::DictionaryValue& params) {
    listener_->OnEvent(this, method, params);
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

void ValidateLogEntry(LogEntry *entry,
                      Log::Level expect_level,
                      const char* expect_message) {
  EXPECT_EQ(expect_level, entry->level);
  EXPECT_LT(0, entry->timestamp.ToTimeT());
  EXPECT_STREQ(expect_message, entry->message.c_str());
}

void ConsoleLogParams(base::DictionaryValue* out_params,
                      const char* source,
                      const char* url,
                      const char* level,
                      int line,
                      int column,
                      const char* text) {
  if (NULL != source)
    out_params->SetString("message.source", source);
  if (NULL != url)
    out_params->SetString("message.url", url);
  if (NULL != level)
    out_params->SetString("message.level", level);
  if (-1 != line)
    out_params->SetInteger("message.line", line);
  if (-1 != column)
    out_params->SetInteger("message.column", column);
  if (NULL != text)
    out_params->SetString("message.text", text);
}

}  // namespace

TEST(ConsoleLogger, ConsoleMessages) {
  FakeDevToolsClient client("webview");
  FakeLog log;
  ConsoleLogger logger(&log);

  client.AddListener(&logger);
  logger.OnConnected(&client);
  EXPECT_STREQ("Console.enable", client.PopSentCommand().c_str());
  EXPECT_STREQ("", client.PopSentCommand().c_str());

  base::DictionaryValue params1;  // All fields are set.
  ConsoleLogParams(&params1, "source1", "url1", "debug", 10, 1, "text1");
  client.TriggerEvent("Console.messageAdded", params1);
  client.TriggerEvent("Console.gaga", params1);  // Ignored -- wrong method.

  base::DictionaryValue params2;  // All optionals are not set.
  ConsoleLogParams(&params2, "source2", NULL, "log", -1, -1, "text2");
  client.TriggerEvent("Console.messageAdded", params2);

  base::DictionaryValue params3;  // Line without column, no source.
  ConsoleLogParams(&params3, NULL, "url3", "warning", 30, -1, "text3");
  client.TriggerEvent("Console.messageAdded", params3);

  base::DictionaryValue params4;  // Column without line.
  ConsoleLogParams(&params4, "source4", "url4", "error", -1, 4, "text4");
  client.TriggerEvent("Console.messageAdded", params4);

  base::DictionaryValue params5;  // Bad level name.
  ConsoleLogParams(&params5, "source5", "url5", "gaga", 50, 5, "ulala");
  client.TriggerEvent("Console.messageAdded", params5);

  base::DictionaryValue params6;  // Unset level.
  ConsoleLogParams(&params6, "source6", "url6", NULL, 60, 6, NULL);
  client.TriggerEvent("Console.messageAdded", params6);

  base::DictionaryValue params7;  // No text.
  ConsoleLogParams(&params7, "source7", "url7", "log", -1, -1, NULL);
  client.TriggerEvent("Console.messageAdded", params7);

  base::DictionaryValue params8;  // No message object.
  params8.SetInteger("gaga", 8);
  client.TriggerEvent("Console.messageAdded", params8);

  EXPECT_STREQ("", client.PopSentCommand().c_str());  // No other commands sent.

  ASSERT_EQ(8u, log.entries.size());
  ValidateLogEntry(log.entries[0], Log::kDebug, "url1 10:1 text1");
  ValidateLogEntry(log.entries[1], Log::kLog, "source2 - text2");
  ValidateLogEntry(log.entries[2], Log::kWarning, "url3 30 text3");
  ValidateLogEntry(log.entries[3], Log::kError, "url4 - text4");
  ValidateLogEntry(
      log.entries[4], Log::kWarning,
      "{\"message\":{\"column\":5,\"level\":\"gaga\",\"line\":50,"
      "\"source\":\"source5\",\"text\":\"ulala\",\"url\":\"url5\"}}");
  ValidateLogEntry(
      log.entries[5], Log::kWarning,
      "{\"message\":{\"column\":6,\"line\":60,"
      "\"source\":\"source6\",\"url\":\"url6\"}}");
  ValidateLogEntry(
      log.entries[6], Log::kWarning,
      "{\"message\":{\"level\":\"log\","
      "\"source\":\"source7\",\"url\":\"url7\"}}");
  ValidateLogEntry(log.entries[7], Log::kWarning, "{\"gaga\":8}");
}
