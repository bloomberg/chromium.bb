// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_installer/win/app_installer_util.h"

#include <map>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace app_installer {

TEST(AppInstallerUtilTest, ParseTag) {
  std::map<std::string, std::string> parsed_pairs;

  parsed_pairs.clear();
  EXPECT_TRUE(ParseTag("key1=value1&key2=value2", &parsed_pairs));
  EXPECT_EQ(2, parsed_pairs.size());
  EXPECT_EQ("value1", parsed_pairs["key1"]);
  EXPECT_EQ("value2", parsed_pairs["key2"]);

  parsed_pairs.clear();
  EXPECT_FALSE(ParseTag("a&b", &parsed_pairs));

  parsed_pairs.clear();
  EXPECT_FALSE(ParseTag("#=a", &parsed_pairs));

  parsed_pairs.clear();
  EXPECT_FALSE(ParseTag("a=\01", &parsed_pairs));
}

}  // namespace app_installer
