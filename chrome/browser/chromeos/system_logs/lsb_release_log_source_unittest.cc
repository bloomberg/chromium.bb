// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/lsb_release_log_source.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(LSBReleaseLogSource, BasicFunctionality) {
  std::string data("key=value\nkey2 =  value2   ");
  SystemLogsResponse response;
  LsbReleaseLogSource::ParseLSBRelease(data, &response);
  EXPECT_EQ(2U, response.size());
  EXPECT_EQ("value", response["key"]);

  // Make sure extra white space is removed correctly. First from the key and
  // then from the value.
  EXPECT_EQ(1U, response.count("key2"));
  EXPECT_EQ("value2", response["key2"]);
}

TEST(LSBReleaseLogSource, ValueMissing) {
  std::string data("key=value\nkey2=\nkey3");
  SystemLogsResponse response;
  LsbReleaseLogSource::ParseLSBRelease(data, &response);
  EXPECT_EQ(2U, response.size());
  EXPECT_EQ("value", response["key"]);
  EXPECT_EQ("<no value>", response["key2"]);
  EXPECT_EQ(0U, response.count("key3"));
}

TEST(LSBReleaseLogSource, BrokenUTF8) {
  std::string data("key=value\nkey2=\nkey3=\xFCts\nkey4=value4\nkey5\xFC=foo");
  SystemLogsResponse response;
  LsbReleaseLogSource::ParseLSBRelease(data, &response);
  EXPECT_EQ("<invalid characters in log entry>", response["key3"]);
  EXPECT_EQ("value4", response["key4"]);
  EXPECT_EQ(0U, response.count("key5"));
}

TEST(LSBReleasLogSource, NoKeyValuePair) {
  SystemLogsResponse response;

  std::string contents1("random text without a meaning");
  LsbReleaseLogSource::ParseLSBRelease(contents1, &response);
  EXPECT_TRUE(response.empty());
}

TEST(LSBReleasLogSource, EmptyInput) {
  SystemLogsResponse response;
  std::string contents2("");
  LsbReleaseLogSource::ParseLSBRelease(contents2, &response);
  EXPECT_TRUE(response.empty());
}

}  // namespace chromeos
