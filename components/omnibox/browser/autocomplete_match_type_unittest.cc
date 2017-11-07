// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match_type.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(AutocompleteMatchTypeTest, AccessibilityLabels) {
  const base::string16& kTestUrl =
      base::UTF8ToUTF16("https://www.chromium.org");
  EXPECT_EQ(kTestUrl, AutocompleteMatchType::ToAccessibilityLabel(
                          AutocompleteMatchType::URL_WHAT_YOU_TYPED, kTestUrl));
  EXPECT_EQ(base::UTF8ToUTF16("URL from history: ") + kTestUrl,
            AutocompleteMatchType::ToAccessibilityLabel(
                AutocompleteMatchType::HISTORY_URL, kTestUrl));
}
