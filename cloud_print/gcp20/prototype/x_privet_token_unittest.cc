// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/x_privet_token.h"

#include <stdio.h>

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(XPrivetTokenTest, Generation) {
  std::string secret = "E3E92296-E290-4E77-B678-6AEF256C30C8";
  uint64 gen_time = 1372444784;
  uint64 issue_time = gen_time;

  XPrivetToken xtoken(secret, base::Time::FromTimeT(gen_time));

  std::string issue_time_str = base::StringPrintf("%" PRIu64, issue_time);
  std::string sha1_val = base::SHA1HashString(secret + ":" + issue_time_str);

  ASSERT_STRCASEEQ("2216828f9eefc3931c1b9a110dcca3dbec23571d",
                   base::HexEncode(sha1_val.data(), sha1_val.size()).c_str());

  std::string base64_val;
  base::Base64Encode(sha1_val, &base64_val);
  std::string token = base64_val + ":" + issue_time_str;

  ASSERT_EQ(token, xtoken.GenerateXTokenWithTime(issue_time));

  EXPECT_NE(xtoken.GenerateXTokenWithTime(issue_time),
            xtoken.GenerateXTokenWithTime(issue_time + 1));
}

TEST(XPrivetTokenTest, CheckingValid) {
  base::Time gen_time = base::Time::FromTimeT(1372444784);
  XPrivetToken xtoken("9CEEA1AD-BC24-4D5A-8AB4-A6CE3E0CC4CD", gen_time);

  std::string token = xtoken.GenerateXTokenWithTime(gen_time.ToTimeT());
  EXPECT_TRUE(xtoken.CheckValidXToken(token));

  token = xtoken.GenerateXTokenWithTime(gen_time.ToTimeT() + 1);
  EXPECT_TRUE(xtoken.CheckValidXToken(token));

  token = xtoken.GenerateXTokenWithTime(gen_time.ToTimeT() + 55);
  EXPECT_TRUE(xtoken.CheckValidXToken(token));

  token = xtoken.GenerateXTokenWithTime(gen_time.ToTimeT() + 60*60 - 5);
  EXPECT_TRUE(xtoken.CheckValidXToken(token));

  token = xtoken.GenerateXTokenWithTime(gen_time.ToTimeT() + 60*60 + 10);
  EXPECT_TRUE(xtoken.CheckValidXToken(token));

  token = xtoken.GenerateXTokenWithTime(gen_time.ToTimeT() + 24*60*60 - 1);
  EXPECT_TRUE(xtoken.CheckValidXToken(token));
}

TEST(XPrivetTokenTest, CheckingInvalid) {
  base::Time gen_time = base::Time::FromTimeT(1372444784);
  XPrivetToken xtoken("9CEEA1AD-BC24-4D5A-8AB4-A6CE3E0CC4CD", gen_time);

  // Meaningless tokens:

  std::string token = "CEEA1AD9CEEA1AD9CEEA1AD9CEEA1AD";
  EXPECT_FALSE(xtoken.CheckValidXToken(token));

  base::Base64Encode("CEEA1AD9CEEA1AD9CEEA1AD9CEEA1AD", &token);
  EXPECT_FALSE(xtoken.CheckValidXToken(token));

  base::Base64Encode("CEEA1AD9CEEA:1AD9CEEA1AD9CEEA1AD", &token);
  EXPECT_FALSE(xtoken.CheckValidXToken(token));

  base::Base64Encode("CEEA1AD9CEEA:1AD9CEEA1AD9:CEEA1AD", &token);
  EXPECT_FALSE(xtoken.CheckValidXToken(token));

  base::Base64Encode("CEEA1AD9CEEA:123456", &token);
  EXPECT_FALSE(xtoken.CheckValidXToken(token));

  base::Base64Encode("CEEA1AD9CEEA:", &token);
  EXPECT_FALSE(xtoken.CheckValidXToken(token));

  base::Base64Encode("CEEA1AD9CEEA:1372444784", &token);
  EXPECT_FALSE(xtoken.CheckValidXToken(token));

  EXPECT_FALSE(xtoken.CheckValidXToken(""));

  // Expired tokens:

  token = xtoken.GenerateXTokenWithTime(gen_time.ToTimeT() + 24*60*60 + 1);
  EXPECT_FALSE(xtoken.CheckValidXToken(token));

  token = xtoken.GenerateXTokenWithTime(gen_time.ToTimeT() + 7*24*60*60 - 1023);
  EXPECT_FALSE(xtoken.CheckValidXToken(token));

  // Tokens with different secret:

  XPrivetToken another("6F02AC4E-6F37-4078-AF42-5EE5D8180284", gen_time);

  token = another.GenerateXTokenWithTime(gen_time.ToTimeT() - 24*60*60 - 1);
  EXPECT_FALSE(xtoken.CheckValidXToken(token));

  token = another.GenerateXTokenWithTime(gen_time.ToTimeT() - 24*60*60 + 1);
  EXPECT_FALSE(xtoken.CheckValidXToken(token));

  token = another.GenerateXTokenWithTime(gen_time.ToTimeT());
  EXPECT_FALSE(xtoken.CheckValidXToken(token));

  token = another.GenerateXTokenWithTime(gen_time.ToTimeT() + 1);
  EXPECT_FALSE(xtoken.CheckValidXToken(token));

  token = another.GenerateXTokenWithTime(gen_time.ToTimeT() + 24*60*60 - 1);
  EXPECT_FALSE(xtoken.CheckValidXToken(token));

  token = another.GenerateXTokenWithTime(gen_time.ToTimeT() + 24*60*60 + 1);
  EXPECT_FALSE(xtoken.CheckValidXToken(token));
}

