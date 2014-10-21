// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/pref_service_bridge.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"


// A test class for PrefServiceBridge class.
class PrefServiceBridgeTest : public testing::Test {
 public:
  std::string GetAcceptLanguages(std::string locale,
                                 std::string accept_languages) {
    PrependToAcceptLanguagesIfNecessary(locale, &accept_languages);
    return accept_languages;
  }
};

TEST_F(PrefServiceBridgeTest, PrependToAcceptLanguagesAsNecessary) {
  EXPECT_EQ("ms-MY,ms,en-US,en", GetAcceptLanguages("ms_MY", "en-US,en"));

  EXPECT_EQ("de-CH,de-DE,de,en-US,en",
      GetAcceptLanguages("de_CH", "de-DE,de,en-US,en"));

  // Make sure we do not prepend language code even when a language code is at
  // the end.
  EXPECT_EQ("zh-TW,zh-CN,zh", GetAcceptLanguages("zh_TW", "zh-CN,zh"));
}

TEST_F(PrefServiceBridgeTest,
       ShouldNotPrependToAcceptLanguagesWhenNotNecessary) {
  // Java has some deprecated two letter language code. For instance, "iw" is
  // an old code that needs to be replaced by "he" which is the new code for
  // Hebrew.
  EXPECT_EQ("he-IL,he,en-US,en",
      GetAcceptLanguages("iw_IL", "he-IL,he,en-US,en"));

  // This logic should not affect cases where original accept language already
  // reflects the language code in the locale.
  EXPECT_EQ("en-US,en", GetAcceptLanguages("en_US", "en-US,en"));
  EXPECT_EQ("zh-CN,zh", GetAcceptLanguages("zh_CN", "zh-CN,zh"));

  // "ms" is not expected. No replacement takes place.
  EXPECT_EQ("en-US,en", GetAcceptLanguages("ms", "en-US,en"));
}
