// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/logging.h"

#include "base/values.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char* const kAllWdLevels[] = {
  "ALL", "DEBUG", "INFO", "WARNING", "SEVERE", "OFF"
};

}

TEST(Logging, NameLevelConversionHappy) {
  // All names map to a valid enum value.
  for (int i = 0; static_cast<size_t>(i) < arraysize(kAllWdLevels); ++i) {
    WebDriverLog::WebDriverLevel level =
        static_cast<WebDriverLog::WebDriverLevel>(-1);
    EXPECT_TRUE(WebDriverLog::NameToLevel(kAllWdLevels[i], &level));
    EXPECT_LE(WebDriverLog::kWdAll, level);
    EXPECT_GE(WebDriverLog::kWdOff, level);
  }
}

TEST(Logging, NameToLevelErrors) {
  WebDriverLog::WebDriverLevel level =
      static_cast<WebDriverLog::WebDriverLevel>(-1);
  EXPECT_FALSE(WebDriverLog::NameToLevel("A", &level));
  EXPECT_FALSE(WebDriverLog::NameToLevel("B", &level));
  EXPECT_FALSE(WebDriverLog::NameToLevel("H", &level));
  EXPECT_FALSE(WebDriverLog::NameToLevel("R", &level));
  EXPECT_FALSE(WebDriverLog::NameToLevel("T", &level));
  EXPECT_FALSE(WebDriverLog::NameToLevel("Z", &level));
  // The level variable was never modified.
  EXPECT_EQ(static_cast<WebDriverLog::WebDriverLevel>(-1), level);
}

namespace {

void ValidateLogEntry(base::ListValue *entries,
                      int index,
                      const std::string& expected_level,
                      const std::string& expected_message) {
  const base::DictionaryValue *entry;
  ASSERT_TRUE(entries->GetDictionary(index, &entry));
  std::string level;
  EXPECT_TRUE(entry->GetString("level", &level));
  EXPECT_EQ(expected_level, level);
  std::string message;
  ASSERT_TRUE(entry->GetString("message", &message));
  EXPECT_EQ(expected_message, message);
  double timestamp = 0;
  EXPECT_TRUE(entry->GetDouble("timestamp", &timestamp));
  EXPECT_LT(0, timestamp);
}

}

TEST(WebDriverLog, Levels) {
  WebDriverLog log("type", WebDriverLog::kWdInfo);
  log.AddEntry(Log::kLog, std::string("info message"));
  log.AddEntry(Log::kError, "severe message");
  log.AddEntry(Log::kDebug, "debug message");  // Must not log

  scoped_ptr<base::ListValue> entries(log.GetAndClearEntries());

  ASSERT_EQ(2u, entries->GetSize());
  ValidateLogEntry(entries.get(), 0, "INFO", "info message");
  ValidateLogEntry(entries.get(), 1, "SEVERE", "severe message");
}

TEST(WebDriverLog, Off) {
  WebDriverLog log("type", WebDriverLog::kWdOff);
  log.AddEntry(Log::kError, "severe message");  // Must not log
  log.AddEntry(Log::kDebug, "debug message");  // Must not log

  scoped_ptr<base::ListValue> entries(log.GetAndClearEntries());

  ASSERT_EQ(0u, entries->GetSize());
}

TEST(WebDriverLog, All) {
  WebDriverLog log("type", WebDriverLog::kWdAll);
  log.AddEntry(Log::kError, "severe message");
  log.AddEntry(Log::kDebug, "debug message");

  scoped_ptr<base::ListValue> entries(log.GetAndClearEntries());

  ASSERT_EQ(2u, entries->GetSize());
  ValidateLogEntry(entries.get(), 0, "SEVERE", "severe message");
  ValidateLogEntry(entries.get(), 1, "DEBUG", "debug message");
}

TEST(Logging, CreatePerformanceLog) {
  Capabilities capabilities;
  capabilities.logging_prefs.reset(new base::DictionaryValue());
  capabilities.logging_prefs->SetString("performance", "INFO");

  ScopedVector<DevToolsEventListener> listeners;
  ScopedVector<WebDriverLog> logs;
  Status status = CreateLogs(capabilities, &logs, &listeners);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, logs.size());
  ASSERT_EQ(2u, listeners.size());
  ASSERT_EQ("performance", logs[0]->GetType());
  ASSERT_EQ("browser", logs[1]->GetType());  // Always created.
}

TEST(Logging, CreateBrowserLogOff) {
  Capabilities capabilities;
  capabilities.logging_prefs.reset(new base::DictionaryValue());
  capabilities.logging_prefs->SetString("browser", "OFF");

  ScopedVector<DevToolsEventListener> listeners;
  ScopedVector<WebDriverLog> logs;
  Status status = CreateLogs(capabilities, &logs, &listeners);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, logs.size());
  ASSERT_EQ(0u, listeners.size());
  ASSERT_EQ("browser", logs[0]->GetType());

  // Verify the created log is "OFF" -- drops all messages.
  logs[0]->AddEntry(Log::kError, "drop even errors");
  scoped_ptr<base::ListValue> entries(logs[0]->GetAndClearEntries());
  ASSERT_EQ(0u, entries->GetSize());
}

TEST(Logging, IgnoreUnknownLogType) {
  Capabilities capabilities;
  capabilities.logging_prefs.reset(new base::DictionaryValue());
  capabilities.logging_prefs->SetString("gaga", "INFO");

  ScopedVector<DevToolsEventListener> listeners;
  ScopedVector<WebDriverLog> logs;
  Status status = CreateLogs(capabilities, &logs, &listeners);
  EXPECT_TRUE(status.IsOk());
  ASSERT_EQ(1u, logs.size());
  ASSERT_EQ(1u, listeners.size());
  ASSERT_EQ("browser", logs[0]->GetType());
}

TEST(Logging, BrowserLogCreatedWithoutLoggingPrefs) {
  Capabilities capabilities;

  ScopedVector<DevToolsEventListener> listeners;
  ScopedVector<WebDriverLog> logs;
  Status status = CreateLogs(capabilities, &logs, &listeners);
  EXPECT_TRUE(status.IsOk());
  ASSERT_EQ(1u, logs.size());
  ASSERT_EQ(1u, listeners.size());
  ASSERT_EQ("browser", logs[0]->GetType());

  // Verify the created "browser" log is "INFO" level.
  logs[0]->AddEntry(Log::kLog, "info message");
  logs[0]->AddEntry(Log::kDebug, "drop debug message");
  scoped_ptr<base::ListValue> entries(logs[0]->GetAndClearEntries());
  ASSERT_EQ(1u, entries->GetSize());
  ValidateLogEntry(entries.get(), 0, "INFO", "info message");
}
