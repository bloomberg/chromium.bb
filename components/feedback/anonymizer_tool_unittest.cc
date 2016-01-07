// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/anonymizer_tool.h"

#include <gtest/gtest.h>

namespace feedback {

class AnonymizerToolTest : public testing::Test {
 protected:
  std::string AnonymizeMACAddresses(const std::string& input) {
    return anonymizer_.AnonymizeMACAddresses(input);
  }

  std::string AnonymizeCustomPatterns(const std::string& input) {
    return anonymizer_.AnonymizeCustomPatterns(input);
  }

  static std::string AnonymizeCustomPattern(
      const std::string& input,
      const std::string& pattern,
      std::map<std::string, std::string>* space) {
    return AnonymizerTool::AnonymizeCustomPattern(input, pattern, space);
  }

  AnonymizerTool anonymizer_;
};

TEST_F(AnonymizerToolTest, Anonymize) {
  EXPECT_EQ("", anonymizer_.Anonymize(""));
  EXPECT_EQ("foo\nbar\n", anonymizer_.Anonymize("foo\nbar\n"));

  // Make sure MAC address anonymization is invoked.
  EXPECT_EQ("02:46:8a:00:00:01", anonymizer_.Anonymize("02:46:8a:ce:13:57"));

  // Make sure custom pattern anonymization is invoked.
  EXPECT_EQ("Cell ID: '1'", AnonymizeCustomPatterns("Cell ID: 'A1B2'"));
}

TEST_F(AnonymizerToolTest, AnonymizeMACAddresses) {
  EXPECT_EQ("", AnonymizeMACAddresses(""));
  EXPECT_EQ("foo\nbar\n", AnonymizeMACAddresses("foo\nbar\n"));
  EXPECT_EQ("11:22:33:44:55", AnonymizeMACAddresses("11:22:33:44:55"));
  EXPECT_EQ("aa:bb:cc:00:00:01", AnonymizeMACAddresses("aa:bb:cc:dd:ee:ff"));
  EXPECT_EQ(
      "BSSID: aa:bb:cc:00:00:01 in the middle\n"
      "bb:cc:dd:00:00:02 start of line\n"
      "end of line aa:bb:cc:00:00:01\n"
      "no match across lines aa:bb:cc:\n"
      "dd:ee:ff two on the same line:\n"
      "x bb:cc:dd:00:00:02 cc:dd:ee:00:00:03 x\n",
      AnonymizeMACAddresses("BSSID: aa:bb:cc:dd:ee:ff in the middle\n"
                            "bb:cc:dd:ee:ff:00 start of line\n"
                            "end of line aa:bb:cc:dd:ee:ff\n"
                            "no match across lines aa:bb:cc:\n"
                            "dd:ee:ff two on the same line:\n"
                            "x bb:cc:dd:ee:ff:00 cc:dd:ee:ff:00:11 x\n"));
  EXPECT_EQ("Remember bb:cc:dd:00:00:02?",
            AnonymizeMACAddresses("Remember bB:Cc:DD:ee:ff:00?"));
}

TEST_F(AnonymizerToolTest, AnonymizeCustomPatterns) {
  EXPECT_EQ("", AnonymizeCustomPatterns(""));

  EXPECT_EQ("Cell ID: '1'", AnonymizeCustomPatterns("Cell ID: 'A1B2'"));
  EXPECT_EQ("Cell ID: '2'", AnonymizeCustomPatterns("Cell ID: 'C1D2'"));
  EXPECT_EQ("foo Cell ID: '1' bar",
            AnonymizeCustomPatterns("foo Cell ID: 'A1B2' bar"));

  EXPECT_EQ("foo Location area code: '1' bar",
            AnonymizeCustomPatterns("foo Location area code: 'A1B2' bar"));

  EXPECT_EQ("foo\na SSID='1' b\n'",
            AnonymizeCustomPatterns("foo\na SSID='Joe's' b\n'"));
  EXPECT_EQ("ssid '2'", AnonymizeCustomPatterns("ssid 'My AP'"));
  EXPECT_EQ("bssid 'aa:bb'", AnonymizeCustomPatterns("bssid 'aa:bb'"));

  EXPECT_EQ("Scan SSID - hexdump(len=6): 1\nfoo",
            AnonymizeCustomPatterns(
                "Scan SSID - hexdump(len=6): 47 6f 6f 67 6c 65\nfoo"));

  EXPECT_EQ(
      "a\nb [SSID=1] [SSID=2] [SSID=foo\nbar] b",
      AnonymizeCustomPatterns("a\nb [SSID=foo] [SSID=bar] [SSID=foo\nbar] b"));
}

TEST_F(AnonymizerToolTest, AnonymizeCustomPattern) {
  const char kPattern[] = "(\\b(?i)id:? ')(\\d+)(')";
  std::map<std::string, std::string> space;
  EXPECT_EQ("", AnonymizeCustomPattern("", kPattern, &space));
  EXPECT_EQ("foo\nbar\n",
            AnonymizeCustomPattern("foo\nbar\n", kPattern, &space));
  EXPECT_EQ("id '1'", AnonymizeCustomPattern("id '2345'", kPattern, &space));
  EXPECT_EQ("id '2'", AnonymizeCustomPattern("id '1234'", kPattern, &space));
  EXPECT_EQ("id: '2'", AnonymizeCustomPattern("id: '1234'", kPattern, &space));
  EXPECT_EQ("ID: '1'", AnonymizeCustomPattern("ID: '2345'", kPattern, &space));
  EXPECT_EQ("x1 id '1' 1x id '2'\nid '1'\n",
            AnonymizeCustomPattern("x1 id '2345' 1x id '1234'\nid '2345'\n",
                                   kPattern, &space));
  space.clear();
  EXPECT_EQ("id '1'", AnonymizeCustomPattern("id '1234'", kPattern, &space));

  space.clear();
  EXPECT_EQ("x1z", AnonymizeCustomPattern("xyz", "()(y+)()", &space));
}

}  // namespace feedback
