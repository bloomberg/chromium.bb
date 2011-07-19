// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/extensions/extension_omnibox_api.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

const int kNone = ACMatchClassification::NONE;
const int kUrl = ACMatchClassification::URL;
const int kMatch = ACMatchClassification::MATCH;
const int kDim = ACMatchClassification::DIM;

void AppendStyle(const std::string& type,
                 int offset, int length,
                 ListValue* styles) {
  DictionaryValue* style = new DictionaryValue;
  style->SetString("type", type);
  style->SetInteger("offset", offset);
  style->SetInteger("length", length);
  styles->Append(style);
}

void CompareClassification(const ACMatchClassifications& expected,
                           const ACMatchClassifications& actual) {
  EXPECT_EQ(expected.size(), actual.size());
  for (size_t i = 0; i < expected.size() && i < actual.size(); ++i) {
    EXPECT_EQ(expected[i].offset, actual[i].offset) << "Index:" << i;
    EXPECT_EQ(expected[i].style, actual[i].style) << "Index:" << i;
  }
}

}  // namespace

// Test output key: n = character with no styling, d = dim, m = match, u = url
// u = 1, m = 2, d = 4. u+d = 5, etc.

//   0123456789
//    mmmm
// +       ddd
// = nmmmmndddn
TEST(ExtensionOmniboxTest, DescriptionStylesSimple) {
  ListValue styles_value;
  AppendStyle("match", 1, 4, &styles_value);
  AppendStyle("dim", 6, 3, &styles_value);

  ACMatchClassifications styles_expected;
  styles_expected.push_back(ACMatchClassification(0, kNone));
  styles_expected.push_back(ACMatchClassification(1, kMatch));
  styles_expected.push_back(ACMatchClassification(5, kNone));
  styles_expected.push_back(ACMatchClassification(6, kDim));
  styles_expected.push_back(ACMatchClassification(9, kNone));

  ExtensionOmniboxSuggestion suggestions;
  suggestions.description.resize(10);
  EXPECT_TRUE(suggestions.ReadStylesFromValue(styles_value));
  CompareClassification(styles_expected, suggestions.description_styles);

  // Same input, but swap the order. Ensure it still works.
  styles_value.Clear();
  AppendStyle("dim", 6, 3, &styles_value);
  AppendStyle("match", 1, 4, &styles_value);
  EXPECT_TRUE(suggestions.ReadStylesFromValue(styles_value));
  CompareClassification(styles_expected, suggestions.description_styles);
}

//   0123456789
//   uuuuu
// +          dd
// +          mm
// + mmmm
// +  dd
// = 3773unnnn66
TEST(ExtensionOmniboxTest, DescriptionStylesCombine) {
  ListValue styles_value;
  AppendStyle("url", 0, 5, &styles_value);
  AppendStyle("dim", 9, 2, &styles_value);
  AppendStyle("match", 9, 2, &styles_value);
  AppendStyle("match", 0, 4, &styles_value);
  AppendStyle("dim", 1, 2, &styles_value);

  ACMatchClassifications styles_expected;
  styles_expected.push_back(ACMatchClassification(0, kUrl | kMatch));
  styles_expected.push_back(ACMatchClassification(1, kUrl | kMatch | kDim));
  styles_expected.push_back(ACMatchClassification(3, kUrl | kMatch));
  styles_expected.push_back(ACMatchClassification(4, kUrl));
  styles_expected.push_back(ACMatchClassification(5, kNone));
  styles_expected.push_back(ACMatchClassification(9, kMatch | kDim));

  ExtensionOmniboxSuggestion suggestions;
  suggestions.description.resize(10);
  EXPECT_TRUE(suggestions.ReadStylesFromValue(styles_value));
  CompareClassification(styles_expected, suggestions.description_styles);

  // Try moving the "dim/match" style pair at offset 9. Output should be the
  // same.
  styles_value.Clear();
  AppendStyle("url", 0, 5, &styles_value);
  AppendStyle("match", 0, 4, &styles_value);
  AppendStyle("dim", 9, 2, &styles_value);
  AppendStyle("match", 9, 2, &styles_value);
  AppendStyle("dim", 1, 2, &styles_value);
  EXPECT_TRUE(suggestions.ReadStylesFromValue(styles_value));
  CompareClassification(styles_expected, suggestions.description_styles);
}

//   0123456789
//   uuuuu
// + mmmmm
// + mmm
// +   ddd
// + ddd
// = 77777nnnnn
TEST(ExtensionOmniboxTest, DescriptionStylesCombine2) {
  ListValue styles_value;
  AppendStyle("url", 0, 5, &styles_value);
  AppendStyle("match", 0, 5, &styles_value);
  AppendStyle("match", 0, 3, &styles_value);
  AppendStyle("dim", 2, 3, &styles_value);
  AppendStyle("dim", 0, 3, &styles_value);

  ACMatchClassifications styles_expected;
  styles_expected.push_back(ACMatchClassification(0, kUrl | kMatch | kDim));
  styles_expected.push_back(ACMatchClassification(5, kNone));

  ExtensionOmniboxSuggestion suggestions;
  suggestions.description.resize(10);
  EXPECT_TRUE(suggestions.ReadStylesFromValue(styles_value));
  CompareClassification(styles_expected, suggestions.description_styles);
}
