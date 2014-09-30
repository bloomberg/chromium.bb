// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/browser_info.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

void AssertParseBrowserInfoFails(const std::string& data) {
  BrowserInfo browser_info;
  Status status = ParseBrowserInfo(data, &browser_info);
  ASSERT_TRUE(status.IsError());
}

}  // namespace

TEST(ParseBrowserInfo, InvalidJSON) {
  AssertParseBrowserInfoFails("[");
}

TEST(ParseBrowserInfo, NonDict) {
  AssertParseBrowserInfoFails("[]");
}

TEST(ParseBrowserInfo, NoBrowserKey) {
  AssertParseBrowserInfoFails("{}");
}

TEST(ParseBrowserInfo, BlinkVersionContainsSvnRevision) {
  std::string data("{\"Browser\": \"Chrome/37.0.2062.124\","
                   " \"WebKit-Version\": \"537.36 (@181352)\"}");
  BrowserInfo browser_info;
  Status status = ParseBrowserInfo(data, &browser_info);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ("chrome", browser_info.browser_name);
  ASSERT_EQ("37.0.2062.124", browser_info.browser_version);
  ASSERT_EQ(2062, browser_info.build_no);
  ASSERT_EQ(181352, browser_info.blink_revision);
}

TEST(ParseBrowserInfo, BlinkVersionContainsGitHash) {
  std::string data("{\"Browser\": \"Chrome/37.0.2062.124\","
                   " \"WebKit-Version\":"
                   " \"537.36 (@28f741cfcabffe68a9c12c4e7152569c906bd88f)\"}");
  BrowserInfo browser_info;
  const int default_blink_revision = browser_info.blink_revision;
  Status status = ParseBrowserInfo(data, &browser_info);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ("chrome", browser_info.browser_name);
  ASSERT_EQ("37.0.2062.124", browser_info.browser_version);
  ASSERT_EQ(2062, browser_info.build_no);
  ASSERT_EQ(default_blink_revision, browser_info.blink_revision);
}

TEST(ParseBrowserString, Chrome) {
  std::string browser_name;
  std::string browser_version;
  int build_no;
  Status status = ParseBrowserString(
      "Chrome/37.0.2062.124", &browser_name, &browser_version, &build_no);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ("chrome", browser_name);
  ASSERT_EQ("37.0.2062.124", browser_version);
  ASSERT_EQ(2062, build_no);
}

TEST(ParseBlinkVersionString, GitHash) {
  int rev = -1;
  Status status = ParseBlinkVersionString(
      "537.36 (@28f741cfcabffe68a9c12c4e7152569c906bd88f)", &rev);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(-1, rev);
}

TEST(ParseBlinkVersionString, SvnRevision) {
  int blink_revision = -1;
  Status status = ParseBlinkVersionString("537.36 (@159105)", &blink_revision);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(159105, blink_revision);
}

TEST(IsGitHash, GitHash) {
  ASSERT_TRUE(IsGitHash("28f741cfcabffe68a9c12c4e7152569c906bd88f"));
}

TEST(IsGitHash, GitHashWithUpperCaseCharacters) {
  ASSERT_TRUE(IsGitHash("28F741CFCABFFE68A9C12C4E7152569C906BD88F"));
}

TEST(IsGitHash, SvnRevision) {
  ASSERT_FALSE(IsGitHash("159105"));
}
