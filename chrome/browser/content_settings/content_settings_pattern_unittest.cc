// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_pattern.h"

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(ContentSettingsPatternTest, PatternSupport) {
  EXPECT_TRUE(ContentSettingsPattern("[*.]example.com").IsValid());
  EXPECT_TRUE(ContentSettingsPattern("example.com").IsValid());
  EXPECT_TRUE(ContentSettingsPattern("192.168.0.1").IsValid());
  EXPECT_TRUE(ContentSettingsPattern("[::1]").IsValid());
  EXPECT_TRUE(ContentSettingsPattern("file:///tmp/test.html").IsValid());
  EXPECT_FALSE(ContentSettingsPattern("*example.com").IsValid());
  EXPECT_FALSE(ContentSettingsPattern("example.*").IsValid());
  EXPECT_FALSE(ContentSettingsPattern("http://example.com").IsValid());

  EXPECT_TRUE(ContentSettingsPattern("[*.]example.com").Matches(
              GURL("http://example.com/")));
  EXPECT_TRUE(ContentSettingsPattern("[*.]example.com").Matches(
              GURL("http://www.example.com/")));
  EXPECT_TRUE(ContentSettingsPattern("www.example.com").Matches(
              GURL("http://www.example.com/")));
  EXPECT_TRUE(ContentSettingsPattern("file:///tmp/test.html").Matches(
              GURL("file:///tmp/test.html")));
  EXPECT_FALSE(ContentSettingsPattern("").Matches(
               GURL("http://www.example.com/")));
  EXPECT_FALSE(ContentSettingsPattern("[*.]example.com").Matches(
               GURL("http://example.org/")));
  EXPECT_FALSE(ContentSettingsPattern("example.com").Matches(
               GURL("http://example.org/")));
  EXPECT_FALSE(ContentSettingsPattern("file:///tmp/test.html").Matches(
               GURL("file:///tmp/other.html")));
  EXPECT_FALSE(ContentSettingsPattern("file:///tmp/test.html").Matches(
               GURL("http://example.org/")));
}

TEST(ContentSettingsPatternTest, CanonicalizePattern) {
  // Basic patterns.
  EXPECT_STREQ("[*.]ikea.com", ContentSettingsPattern("[*.]ikea.com")
      .CanonicalizePattern().c_str());
  EXPECT_STREQ("example.com", ContentSettingsPattern("example.com")
      .CanonicalizePattern().c_str());
  EXPECT_STREQ("192.168.1.1", ContentSettingsPattern("192.168.1.1")
      .CanonicalizePattern().c_str());
  EXPECT_STREQ("[::1]", ContentSettingsPattern("[::1]")
      .CanonicalizePattern().c_str());
  EXPECT_STREQ("file:///tmp/file.html", ContentSettingsPattern(
      "file:///tmp/file.html").CanonicalizePattern().c_str());

  // UTF-8 patterns.
  EXPECT_STREQ("[*.]xn--ira-ppa.com", ContentSettingsPattern(
      "[*.]\xC4\x87ira.com").CanonicalizePattern().c_str());
  EXPECT_STREQ("xn--ira-ppa.com", ContentSettingsPattern(
      "\xC4\x87ira.com").CanonicalizePattern().c_str());
  EXPECT_STREQ("file:///%C4%87ira.html", ContentSettingsPattern(
      "file:///\xC4\x87ira.html").CanonicalizePattern().c_str());

  // file:/// normalization.
  EXPECT_STREQ("file:///tmp/test.html", ContentSettingsPattern(
        "file:///tmp/bar/../test.html").CanonicalizePattern().c_str());

  // Invalid patterns.
  EXPECT_STREQ("", ContentSettingsPattern(
      "*example.com").CanonicalizePattern().c_str());
  EXPECT_STREQ("", ContentSettingsPattern(
      "example.*").CanonicalizePattern().c_str());
  EXPECT_STREQ("", ContentSettingsPattern(
      "*\xC4\x87ira.com").CanonicalizePattern().c_str());
  EXPECT_STREQ("", ContentSettingsPattern(
      "\xC4\x87ira.*").CanonicalizePattern().c_str());
}

}  // namespace
