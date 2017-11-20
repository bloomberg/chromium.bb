// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match_type.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(AutocompleteMatchTypeTest, AccessibilityLabels) {
  const base::string16& kTestUrl =
      base::UTF8ToUTF16("https://www.chromium.org");
  const base::string16& kTestTitle = base::UTF8ToUTF16("The Chromium Projects");

  EXPECT_EQ(kTestUrl, AutocompleteMatchType::ToAccessibilityLabel(
                          AutocompleteMatchType::URL_WHAT_YOU_TYPED, kTestUrl,
                          kTestTitle));
  EXPECT_EQ(kTestTitle + base::UTF8ToUTF16(" ") + kTestUrl +
                base::UTF8ToUTF16(" location from history"),
            AutocompleteMatchType::ToAccessibilityLabel(
                AutocompleteMatchType::HISTORY_URL, kTestUrl, kTestTitle));
}
