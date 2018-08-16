// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/test/chromedriver/log_replay/devtools_log_reader.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Log files to test the reader against
const char* const kTestDataPath[] = {"chrome", "test", "chromedriver",
                                     "log_replay", "test_data"};
const char kTestGetTitlePath[] = "testGetTitle_simple.log";
const char kOneEntryPath[] = "oneDevToolsEntry.log";
const char kTruncatedJSONPath[] = "truncatedJSON.log";

base::FilePath GetLogFileFromLiteral(const char literal[]) {
  base::FilePath root_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &root_dir));
  for (int i = 0; i < 5; i++)
    root_dir = root_dir.AppendASCII(kTestDataPath[i]);
  base::FilePath result = root_dir.AppendASCII(literal);
  CHECK(base::PathExists(result));
  return result;
}
}  // namespace

TEST(DevToolsLogReaderTest, Basic) {
  base::FilePath path = GetLogFileFromLiteral(kTestGetTitlePath);
  DevToolsLogReader reader(path);
  std::unique_ptr<LogEntry> next = reader.GetNext(LogEntry::Protocol::HTTP);
  EXPECT_TRUE(next != nullptr);
  EXPECT_EQ(next->protocol_type, LogEntry::Protocol::HTTP);
  EXPECT_EQ(next->command_name, "http://localhost:38845/json/version");
  next = reader.GetNext(LogEntry::Protocol::HTTP);
  EXPECT_TRUE(next != nullptr);
  EXPECT_EQ(next->payload, "{   \"string_key\": \"string_value\"}");
}

TEST(DevToolsLogReaderTest, Multiple) {
  base::FilePath path = GetLogFileFromLiteral(kTestGetTitlePath);
  DevToolsLogReader reader(path);
  std::unique_ptr<LogEntry> next;
  for (int i = 0; i < 3; i++)
    next = reader.GetNext(LogEntry::Protocol::HTTP);

  EXPECT_TRUE(next != nullptr);
  EXPECT_EQ(next->command_name, "http://localhost:38845/json");
  next = reader.GetNext(LogEntry::Protocol::HTTP);
  EXPECT_EQ(next->payload,
            "[ {   \"string_key1\": \"string_value1\"}, {   "
            "\"string_key2\": \"string_value2\"} ]");
}

TEST(DevToolsLogReaderTest, EndOfFile) {
  base::FilePath path = GetLogFileFromLiteral(kOneEntryPath);
  DevToolsLogReader reader(path);
  std::unique_ptr<LogEntry> next = reader.GetNext(LogEntry::Protocol::HTTP);
  EXPECT_TRUE(next != nullptr);
  next = reader.GetNext(LogEntry::Protocol::HTTP);
  EXPECT_TRUE(next == nullptr);
}

TEST(DevToolsLogReaderTest, WebSocketBasic) {
  base::FilePath path = GetLogFileFromLiteral(kTestGetTitlePath);
  DevToolsLogReader reader(path);
  std::unique_ptr<LogEntry> next =
      reader.GetNext(LogEntry::Protocol::WebSocket);
  EXPECT_TRUE(next != nullptr);
  EXPECT_EQ(next->protocol_type, LogEntry::Protocol::WebSocket);
  EXPECT_EQ(next->event_type, LogEntry::EventType::request);
  EXPECT_EQ(next->command_name, "Log.enable");
  EXPECT_EQ(next->id, 1);
}

TEST(DevToolsLogReaderTest, WebSocketMultiple) {
  base::FilePath path = GetLogFileFromLiteral(kTestGetTitlePath);
  DevToolsLogReader reader(path);
  std::unique_ptr<LogEntry> next =
      reader.GetNext(LogEntry::Protocol::WebSocket);
  next = reader.GetNext(LogEntry::Protocol::WebSocket);
  EXPECT_TRUE(next != nullptr);
  EXPECT_EQ(next->event_type, LogEntry::EventType::request);
  EXPECT_EQ(next->command_name, "DOM.getDocument");
  EXPECT_EQ(next->id, 2);
}

TEST(DevToolsLogReaderTest, WebSocketPayload) {
  base::FilePath path = GetLogFileFromLiteral(kTestGetTitlePath);
  DevToolsLogReader reader(path);
  std::unique_ptr<LogEntry> next;
  for (int i = 0; i < 3; i++)
    next = reader.GetNext(LogEntry::Protocol::WebSocket);
  EXPECT_TRUE(next != nullptr);
  EXPECT_EQ(next->command_name, "Target.setAutoAttach");
  EXPECT_EQ(next->id, 3);
  EXPECT_EQ(next->payload,
            "{   \"autoAttach\": true,   \"waitForDebuggerOnStart\": false}");
}

TEST(DevToolsLogReaderTest, TruncatedJSON) {
  base::FilePath path = GetLogFileFromLiteral(kTruncatedJSONPath);
  DevToolsLogReader reader(path);
  std::unique_ptr<LogEntry> next =
      reader.GetNext(LogEntry::Protocol::WebSocket);
  EXPECT_TRUE(next == nullptr);
}
