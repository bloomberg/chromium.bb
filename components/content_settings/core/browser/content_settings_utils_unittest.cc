// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_utils.h"

#include <string>

#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ContentSettingsUtilsTest, ParsePatternString) {
  content_settings::PatternPair pattern_pair;

  pattern_pair = content_settings::ParsePatternString(std::string());
  EXPECT_FALSE(pattern_pair.first.IsValid());
  EXPECT_FALSE(pattern_pair.second.IsValid());

  pattern_pair = content_settings::ParsePatternString(",");
  EXPECT_FALSE(pattern_pair.first.IsValid());
  EXPECT_FALSE(pattern_pair.second.IsValid());

  pattern_pair = content_settings::ParsePatternString("http://www.foo.com");
  EXPECT_TRUE(pattern_pair.first.IsValid());
  EXPECT_EQ(pattern_pair.second, ContentSettingsPattern::Wildcard());

  // This inconsistency is to recover from some broken code.
  pattern_pair = content_settings::ParsePatternString("http://www.foo.com,");
  EXPECT_TRUE(pattern_pair.first.IsValid());
  EXPECT_FALSE(pattern_pair.second.IsValid());

  pattern_pair = content_settings::ParsePatternString(
      "http://www.foo.com,http://www.bar.com");
  EXPECT_TRUE(pattern_pair.first.IsValid());
  EXPECT_TRUE(pattern_pair.second.IsValid());

  pattern_pair = content_settings::ParsePatternString(
      "http://www.foo.com,http://www.bar.com,");
  EXPECT_FALSE(pattern_pair.first.IsValid());
  EXPECT_FALSE(pattern_pair.second.IsValid());

  pattern_pair = content_settings::ParsePatternString(
      "http://www.foo.com,http://www.bar.com,http://www.error.com");
  EXPECT_FALSE(pattern_pair.first.IsValid());
  EXPECT_FALSE(pattern_pair.second.IsValid());
}
