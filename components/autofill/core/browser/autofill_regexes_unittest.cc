// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_regexes.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_regex_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

TEST(AutofillRegexesTest, AutofillRegexes) {
  struct TestCase {
    const char* const input;
    const char* const pattern;
  };

  const TestCase kPositiveCases[] = {
    // Empty pattern
    {"", ""},
    {"Look, ma' -- a non-empty string!", ""},
    // Substring
    {"string", "tri"},
    // Substring at beginning
    {"string", "str"},
    {"string", "^str"},
    // Substring at end
    {"string", "ring"},
    {"string", "ring$"},
    // Case-insensitive
    {"StRiNg", "string"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kPositiveCases); ++i) {
    const TestCase& test_case = kPositiveCases[i];
    SCOPED_TRACE(test_case.input);
    SCOPED_TRACE(test_case.pattern);
    EXPECT_TRUE(autofill::MatchesPattern(ASCIIToUTF16(test_case.input),
                                         ASCIIToUTF16(test_case.pattern)));
  }

  const TestCase kNegativeCases[] = {
    // Empty string
    {"", "Look, ma' -- a non-empty pattern!"},
    // Substring
    {"string", "trn"},
    // Substring at beginning
    {"string", " str"},
    {"string", "^tri"},
    // Substring at end
    {"string", "ring "},
    {"string", "rin$"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kNegativeCases); ++i) {
    const TestCase& test_case = kNegativeCases[i];
    SCOPED_TRACE(test_case.input);
    SCOPED_TRACE(test_case.pattern);
    EXPECT_FALSE(autofill::MatchesPattern(ASCIIToUTF16(test_case.input),
                                          ASCIIToUTF16(test_case.pattern)));
  }
}

}  // namespace autofill
