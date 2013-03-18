// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_event_log.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

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
        output += lines[i].substr(n+2) + '\n';
    }
    return output;
  }

  size_t CountLines(const std::string& input) {
    return std::count(input.begin(), input.end(), '\n');
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkEventLogTest);
};

TEST_F(NetworkEventLogTest, TestNetworkEvents) {
  std::string output_none = network_event_log::GetAsString(
      network_event_log::OLDEST_FIRST, 0);
  EXPECT_EQ("No Log Entries.", output_none);

  network_event_log::AddEntry("module1", "event1", "description1");
  network_event_log::AddEntry("module2", "event2", "description2");
  network_event_log::AddEntry("module3", "event3", "description3");
  network_event_log::AddEntry("module3", "event3", "description3");

  const std::string expected_output_oldest_first(
      "module1:event1: description1\n"
      "module2:event2: description2\n"
      "module3:event3: description3 (2)\n");
  std::string output_oldest_first = network_event_log::GetAsString(
      network_event_log::OLDEST_FIRST, 0);
  output_oldest_first = SkipTime(output_oldest_first);
  EXPECT_EQ(expected_output_oldest_first, output_oldest_first);

  const std::string expected_output_oldest_first_short(
      "module2:event2: description2\n"
      "module3:event3: description3 (2)\n");
  std::string output_oldest_first_short = network_event_log::GetAsString(
      network_event_log::OLDEST_FIRST, 2);
  output_oldest_first_short = SkipTime(output_oldest_first_short);
  EXPECT_EQ(expected_output_oldest_first_short, output_oldest_first_short);

  const std::string expected_output_newest_first(
      "module3:event3: description3 (2)\n"
      "module2:event2: description2\n"
      "module1:event1: description1\n");
  std::string output_newest_first = network_event_log::GetAsString(
      network_event_log::NEWEST_FIRST, 0);
  output_newest_first = SkipTime(output_newest_first);
  EXPECT_EQ(expected_output_newest_first, output_newest_first);

  const std::string expected_output_newest_first_short(
      "module3:event3: description3 (2)\n"
      "module2:event2: description2\n");
  std::string output_newest_first_short = network_event_log::GetAsString(
      network_event_log::NEWEST_FIRST, 2);
  output_newest_first_short = SkipTime(output_newest_first_short);
  EXPECT_EQ(expected_output_newest_first_short, output_newest_first_short);
}

TEST_F(NetworkEventLogTest, TestMaxNetworkEvents) {
  const size_t entries_to_add =
      network_event_log::kMaxNetworkEventLogEntries + 3;
  for (size_t i = 0; i < entries_to_add; ++i)
    network_event_log::AddEntry("test",
                                base::StringPrintf("event_%"PRIuS, i), "");

  std::string output = GetAsString(network_event_log::OLDEST_FIRST, 0);
  size_t output_lines = CountLines(output);
  EXPECT_EQ(network_event_log::kMaxNetworkEventLogEntries, output_lines);
}

}  // namespace chromeos
