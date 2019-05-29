// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_parser.h"

#include "base/optional.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

TEST(SmsParserTest, NoToken) {
  ASSERT_FALSE(SmsParser::Parse("foo"));
}

TEST(SmsParserTest, WithTokenInvalidUrl) {
  ASSERT_FALSE(SmsParser::Parse("From: foo"));
}

TEST(SmsParserTest, NoSpace) {
  ASSERT_FALSE(SmsParser::Parse("From:https://example.com"));
}

TEST(SmsParserTest, InvalidUrl) {
  ASSERT_FALSE(SmsParser::Parse("From: //example.com"));
}

TEST(SmsParserTest, FtpScheme) {
  ASSERT_FALSE(SmsParser::Parse("From: ftp://example.com"));
}

TEST(SmsParserTest, HttpScheme) {
  ASSERT_FALSE(SmsParser::Parse("From: http://example.com"));
}

TEST(SmsParserTest, Mailto) {
  ASSERT_FALSE(SmsParser::Parse("From: mailto:goto@chromium.org"));
}

TEST(SmsParserTest, Basic) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("From: https://example.com");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, Realistic) {
  base::Optional<url::Origin> origin = SmsParser::Parse(
      "<#> Your OTP is 1234ABC.\nFrom: https://example.com?s3LhKBB0M33");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, Paths) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("From: https://example.com/foobar");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, Message) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("hello world\nFrom: https://example.com");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, Whitespace) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("hello world\nFrom: https://example.com ");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, Newlines) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("hello world\nFrom: https://example.com\n");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, TwoTokens) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("From: https://a.com From: https://b.com");

  GURL url("https://b.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, AppHash) {
  base::Optional<url::Origin> origin = SmsParser::Parse(
      "<#> Hello World\nFrom: https://example.com?s3LhKBB0M33");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

}  // namespace content
