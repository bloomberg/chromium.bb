// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/language_code_locator.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language {

TEST(LanguageCodeLocatorTest, LocatedLanguage) {
  LanguageCodeLocator locator;
  std::vector<std::string> expected_langs = {"hi", "mr", "ur"};
  // Random place in Madhya Pradesh, expected langs should be hi;mr;ur.
  const auto& result = locator.GetLanguageCode(23.0, 80.0);
  EXPECT_EQ(expected_langs, result);
}

TEST(LanguageCodeLocatorTest, NotFoundLanguage) {
  LanguageCodeLocator locator;
  std::vector<std::string> expected_langs = {};
  // Random place outside India.
  const auto& result = locator.GetLanguageCode(10.0, 10.0);
  EXPECT_EQ(expected_langs, result);
}

}  // namespace language
