// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/installer/util/language_selector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const wchar_t* const kExactMatchCandidates[] = {
#if defined(GOOGLE_CHROME_BUILD)
  L"am", L"ar", L"bg", L"bn", L"ca", L"cs", L"da", L"de", L"el", L"en-gb",
  L"en-us", L"es", L"es-419", L"et", L"fa", L"fi", L"fil", L"fr", L"gu", L"hi",
  L"hr", L"hu", L"id", L"it", L"iw", L"ja", L"kn", L"ko", L"lt", L"lv", L"ml",
  L"mr", L"nl", L"no", L"pl", L"pt-br", L"pt-pt", L"ro", L"ru", L"sk", L"sl",
  L"sr", L"sv", L"sw", L"ta", L"te", L"th", L"tr", L"uk", L"vi", L"zh-cn",
  L"zh-tw"
#else
  L"en-us"
#endif
};

const wchar_t* const kAliasMatchCandidates[] = {
#if defined(GOOGLE_CHROME_BUILD)
  L"he", L"nb", L"tl", L"zh-chs",  L"zh-cht", L"zh-hans", L"zh-hant", L"zh-hk",
  L"zh-mk", L"zh-mo"
#else
  // There is only en-us.
  L"en-us"
#endif
};

const wchar_t* const kWildcardMatchCandidates[] = {
  L"en-AU",
#if defined(GOOGLE_CHROME_BUILD)
  L"es-CO", L"pt-AB", L"zh-SG"
#endif
};

}  // namespace

// Test that a language is selected from the system.
TEST(LanguageSelectorTest, DefaultSelection) {
  installer_util::LanguageSelector instance;
  EXPECT_FALSE(instance.matched_candidate().empty());
}

// Test some hypothetical candidate sets.
TEST(LanguageSelectorTest, AssortedSelections) {
  {
    std::wstring candidates[] = {
      L"fr-BE", L"fr", L"en"
    };
    installer_util::LanguageSelector instance(
        std::vector<std::wstring>(&candidates[0],
                                  &candidates[arraysize(candidates)]));
#if defined(GOOGLE_CHROME_BUILD)
    // Expect the exact match to win.
    EXPECT_EQ(L"fr", instance.matched_candidate());
#else
    // Expect the exact match to win.
    EXPECT_EQ(L"en", instance.matched_candidate());
#endif
  }
  {
    std::wstring candidates[] = {
      L"xx-YY", L"cc-Ssss-RR"
    };
    installer_util::LanguageSelector instance(
      std::vector<std::wstring>(&candidates[0],
      &candidates[arraysize(candidates)]));
    // Expect the fallback to win.
    EXPECT_EQ(L"en-us", instance.matched_candidate());
  }
  {
    std::wstring candidates[] = {
      L"zh-SG", L"en-GB"
    };
    installer_util::LanguageSelector instance(
      std::vector<std::wstring>(&candidates[0],
      &candidates[arraysize(candidates)]));
#if defined(GOOGLE_CHROME_BUILD)
    // Expect the alias match to win.
    EXPECT_EQ(L"zh-SG", instance.matched_candidate());
#else
    // Expect the exact match to win.
    EXPECT_EQ(L"en-GB", instance.matched_candidate());
#endif
  }
}

// A fixture for testing sets of single-candidate selections.
class LanguageSelectorMatchCandidateTest
    : public ::testing::TestWithParam<const wchar_t*> {
};

TEST_P(LanguageSelectorMatchCandidateTest, TestMatchCandidate) {
  installer_util::LanguageSelector instance(
    std::vector<std::wstring>(1, std::wstring(GetParam())));
  EXPECT_EQ(GetParam(), instance.matched_candidate());
}

// Test that all existing translations can be found by exact match.
INSTANTIATE_TEST_CASE_P(
    TestExactMatches,
    LanguageSelectorMatchCandidateTest,
    ::testing::ValuesIn(
        &kExactMatchCandidates[0],
        &kExactMatchCandidates[arraysize(kExactMatchCandidates)]));

// Test the alias matches.
INSTANTIATE_TEST_CASE_P(
    TestAliasMatches,
    LanguageSelectorMatchCandidateTest,
    ::testing::ValuesIn(
        &kAliasMatchCandidates[0],
        &kAliasMatchCandidates[arraysize(kAliasMatchCandidates)]));

// Test a few wildcard matches.
INSTANTIATE_TEST_CASE_P(
    TestWildcardMatches,
    LanguageSelectorMatchCandidateTest,
    ::testing::ValuesIn(
        &kWildcardMatchCandidates[0],
        &kWildcardMatchCandidates[arraysize(kWildcardMatchCandidates)]));
