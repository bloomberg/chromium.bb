// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_event_log.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace network_event_log {

namespace {

network_event_log::LogLevel kDefaultLevel = network_event_log::LOG_LEVEL_EVENT;

}  // namespace

class NetworkEventLogTest : public testing::Test {
 public:
  NetworkEventLogTest() {
  }

  virtual void SetUp() OVERRIDE {
    network_event_log::Initialize();
  }

  virtual void TearDown() OVERRIDE {
    network_event_log::Shutdown();
  }

 protected:
  std::string SkipTime(const std::string& input) {
    std::string output;
    std::vector<std::string> lines;
    base::SplitString(input, '\n', &lines);
    for (size_t i = 0; i < lines.size(); ++i) {
      size_t n = lines[i].find(']');
      if (n != std::string::npos)
        output += "[time] " + lines[i].substr(n+2) + '\n';
      else
        output += lines[i];
    }
    return output;
  }

  size_t CountLines(const std::string& input) {
    return std::count(input.begin(), input.end(), '\n');
  }

  std::string GetLogString(StringOrder order, size_t max_events) {
    return network_event_log::GetAsString(
        order, "file,desc", kDefaultLevel, max_events);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkEventLogTest);
};

TEST_F(NetworkEventLogTest, TestNetworkEvents) {
  std::string output_none = GetLogString(OLDEST_FIRST, 0);
  EXPECT_EQ("No Log Entries.", output_none);

  LogLevel level = kDefaultLevel;
  network_event_log::internal::AddEntry(
      "file1", 1, level, "event1", "description1");
  network_event_log::internal::AddEntry(
      "file2", 2, level, "event2", "description2");
  network_event_log::internal::AddEntry(
      "file3", 3, level, "event3", "description3");
  network_event_log::internal::AddEntry(
      "file3", 3, level, "event3", "description3");

  const std::string expected_output_oldest_first(
      "file1:1 event1: description1\n"
      "file2:2 event2: description2\n"
      "file3:3 event3: description3 (2)\n");
  std::string output_oldest_first = GetLogString(OLDEST_FIRST, 0);
  EXPECT_EQ(expected_output_oldest_first, output_oldest_first);

  const std::string expected_output_oldest_first_short(
      "file2:2 event2: description2\n"
      "file3:3 event3: description3 (2)\n");
  std::string output_oldest_first_short = GetLogString(OLDEST_FIRST, 2);
  EXPECT_EQ(expected_output_oldest_first_short, output_oldest_first_short);

  const std::string expected_output_newest_first(
      "file3:3 event3: description3 (2)\n"
      "file2:2 event2: description2\n"
      "file1:1 event1: description1\n");
  std::string output_newest_first = GetLogString(NEWEST_FIRST, 0);
  EXPECT_EQ(expected_output_newest_first, output_newest_first);

  const std::string expected_output_newest_first_short(
      "file3:3 event3: description3 (2)\n"
      "file2:2 event2: description2\n");
  std::string output_newest_first_short = GetLogString(NEWEST_FIRST, 2);
  EXPECT_EQ(expected_output_newest_first_short, output_newest_first_short);
}

TEST_F(NetworkEventLogTest, TestMaxNetworkEvents) {
  const size_t max_entries = network_event_log::internal::GetMaxLogEntries();
  const size_t entries_to_add = max_entries + 3;
  for (size_t i = 0; i < entries_to_add; ++i) {
    network_event_log::internal::AddEntry(
        "test", 1, LOG_LEVEL_EVENT,
        base::StringPrintf("event_%" PRIuS, i), "");
  }
  std::string output = GetLogString(OLDEST_FIRST, 0);
  size_t output_lines = CountLines(output);
  EXPECT_EQ(max_entries, output_lines);
}

TEST_F(NetworkEventLogTest, TestStringFormat) {
  network_event_log::internal::AddEntry(
      "file", 0, LOG_LEVEL_ERROR, "event0", "description");
  EXPECT_EQ("file:0 event0\n", network_event_log::GetAsString(
      OLDEST_FIRST, "file", kDefaultLevel, 1));
  EXPECT_EQ("[time] event0\n", SkipTime(network_event_log::GetAsString(
      OLDEST_FIRST, "time", kDefaultLevel, 1)));
  EXPECT_EQ("event0: description\n", network_event_log::GetAsString(
      OLDEST_FIRST, "desc", kDefaultLevel, 1));
  EXPECT_EQ("event0\n", network_event_log::GetAsString(
      OLDEST_FIRST, "", kDefaultLevel, 1));
  EXPECT_EQ("<b><i>event0</i></b>\n", network_event_log::GetAsString(
      OLDEST_FIRST, "html", kDefaultLevel, 1));
  EXPECT_EQ("[time] file:0 event0: description\n",
            SkipTime(network_event_log::GetAsString(
                OLDEST_FIRST, "file,time,desc", kDefaultLevel, 1)));

  network_event_log::internal::AddEntry(
      "file", 0, LOG_LEVEL_DEBUG, "event1", "description");
  EXPECT_EQ("[time] file:0 <i>event1: description</i>\n",
            SkipTime(network_event_log::GetAsString(
                OLDEST_FIRST, "file,time,desc,html", LOG_LEVEL_DEBUG, 1)));
}

namespace {

void AddTestEvent(LogLevel level, const std::string& event) {
  network_event_log::internal::AddEntry("file", 0, level, event, "description");
}

}  // namespace

TEST_F(NetworkEventLogTest, TestLogLevel) {
  AddTestEvent(LOG_LEVEL_ERROR, "error1");
  AddTestEvent(LOG_LEVEL_ERROR, "error2");
  AddTestEvent(LOG_LEVEL_EVENT, "event3");
  AddTestEvent(LOG_LEVEL_ERROR, "error4");
  AddTestEvent(LOG_LEVEL_EVENT, "event5");
  AddTestEvent(LOG_LEVEL_DEBUG, "debug6");

  std::string output;
  output = network_event_log::GetAsString(
      OLDEST_FIRST, "", LOG_LEVEL_ERROR, 0);
  EXPECT_EQ(3u, CountLines(output));
  output = network_event_log::GetAsString(
      OLDEST_FIRST, "", LOG_LEVEL_EVENT, 0);
  EXPECT_EQ(5u, CountLines(output));
  output = network_event_log::GetAsString(
      OLDEST_FIRST, "", LOG_LEVEL_DEBUG, 0);
  EXPECT_EQ(6u, CountLines(output));

  // Test max_level. Get only the ERROR entries.
  output = network_event_log::GetAsString(
      OLDEST_FIRST, "", LOG_LEVEL_ERROR, 0);
  EXPECT_EQ("error1\nerror2\nerror4\n", output);
}

TEST_F(NetworkEventLogTest, TestMaxEvents) {
  AddTestEvent(LOG_LEVEL_EVENT, "event1");
  AddTestEvent(LOG_LEVEL_ERROR, "error2");
  AddTestEvent(LOG_LEVEL_EVENT, "event3");
  AddTestEvent(LOG_LEVEL_ERROR, "error4");
  AddTestEvent(LOG_LEVEL_EVENT, "event5");

  // Oldest first
  EXPECT_EQ("error4\n", network_event_log::GetAsString(
      OLDEST_FIRST, "", LOG_LEVEL_ERROR, 1));

  EXPECT_EQ("error2\nerror4\n", network_event_log::GetAsString(
      OLDEST_FIRST, "", LOG_LEVEL_ERROR, 2));

  EXPECT_EQ("event3\nerror4\nevent5\n", network_event_log::GetAsString(
      OLDEST_FIRST, "", LOG_LEVEL_EVENT, 3));

  EXPECT_EQ("error2\nevent3\nerror4\nevent5\n", network_event_log::GetAsString(
      OLDEST_FIRST, "", LOG_LEVEL_EVENT, 4));

  EXPECT_EQ("event1\nerror2\nevent3\nerror4\nevent5\n",
            network_event_log::GetAsString(
                OLDEST_FIRST, "", LOG_LEVEL_EVENT, 5));

  // Newest first
  EXPECT_EQ("error4\n", network_event_log::GetAsString(
      NEWEST_FIRST, "", LOG_LEVEL_ERROR, 1));

  EXPECT_EQ("error4\nerror2\n", network_event_log::GetAsString(
      NEWEST_FIRST, "", LOG_LEVEL_ERROR, 2));

  EXPECT_EQ("event5\nerror4\nevent3\n", network_event_log::GetAsString(
      NEWEST_FIRST, "", LOG_LEVEL_EVENT, 3));

  EXPECT_EQ("event5\nerror4\nevent3\nerror2\n", network_event_log::GetAsString(
      NEWEST_FIRST, "", LOG_LEVEL_EVENT, 4));

  EXPECT_EQ("event5\nerror4\nevent3\nerror2\nevent1\n",
            network_event_log::GetAsString(
                NEWEST_FIRST, "", LOG_LEVEL_EVENT, 5));
}

TEST_F(NetworkEventLogTest, TestMaxErrors) {
  network_event_log::internal::SetMaxLogEntries(4);
  AddTestEvent(LOG_LEVEL_EVENT, "event1");
  AddTestEvent(LOG_LEVEL_ERROR, "error2");
  AddTestEvent(LOG_LEVEL_EVENT, "event3");
  AddTestEvent(LOG_LEVEL_ERROR, "error4");
  AddTestEvent(LOG_LEVEL_EVENT, "event5");
  AddTestEvent(LOG_LEVEL_EVENT, "event6");
  EXPECT_EQ("error2\nerror4\nevent5\nevent6\n", network_event_log::GetAsString(
      OLDEST_FIRST, "", LOG_LEVEL_DEBUG, 0));
  network_event_log::internal::SetMaxLogEntries(0);
  EXPECT_EQ("No Log Entries.", network_event_log::GetAsString(
      OLDEST_FIRST, "", LOG_LEVEL_DEBUG, 0));
  network_event_log::internal::SetMaxLogEntries(4);
  AddTestEvent(LOG_LEVEL_ERROR, "error1");
  AddTestEvent(LOG_LEVEL_ERROR, "error2");
  AddTestEvent(LOG_LEVEL_ERROR, "error3");
  AddTestEvent(LOG_LEVEL_ERROR, "error4");
  AddTestEvent(LOG_LEVEL_EVENT, "event5");
  AddTestEvent(LOG_LEVEL_EVENT, "event6");
  EXPECT_EQ("error3\nerror4\nevent5\nevent6\n", network_event_log::GetAsString(
      OLDEST_FIRST, "", LOG_LEVEL_DEBUG, 0));
}

}  // namespace network_event_log
}  // namespace chromeos
