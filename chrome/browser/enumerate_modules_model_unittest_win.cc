// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/enumerate_modules_model_win.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef public testing::Test EnumerateModulesTest;

// Set up some constants to use as default when creating the structs.
static const ModuleEnumerator::ModuleType kType =
    ModuleEnumerator::LOADED_MODULE;

static const ModuleEnumerator::ModuleStatus kStatus =
    ModuleEnumerator::NOT_MATCHED;

static const ModuleEnumerator::RecommendedAction kAction =
    ModuleEnumerator::NONE;

// This is a list of test cases to normalize.
static const struct NormalizationEntryList {
  ModuleEnumerator::Module test_case;
  ModuleEnumerator::Module expected;
} kNormalizationTestCases[] = {
  {
    // Only path normalization needed.
    {kType, kStatus, L"c:\\foo\\bar.dll", L"",        L"Prod", L"Desc", L"1.0",
         L"Sig", kAction},
    {kType, kStatus, L"c:\\foo\\",        L"bar.dll", L"Prod", L"Desc", L"1.0",
         L"Sig", kAction},
  }, {
    // Lower case normalization.
    {kType, kStatus, L"C:\\Foo\\Bar.dll", L"",        L"", L"", L"1.0",
         L"", kAction},
    {kType, kStatus, L"c:\\foo\\",        L"bar.dll", L"", L"", L"1.0",
         L"", kAction},
  }, {
    // Version can include strings after the version number. Strip that away.
    {kType, kStatus, L"c:\\foo.dll", L"",        L"", L"", L"1.0 asdf",
         L"", kAction},
    {kType, kStatus, L"c:\\",        L"foo.dll", L"", L"", L"1.0",
         L"", kAction},
  }, {
    // Corner case: No path (not sure this will ever happen).
    {kType, kStatus, L"bar.dll", L"",        L"", L"", L"", L"", kAction},
    {kType, kStatus, L"",        L"bar.dll", L"", L"", L"", L"", kAction},
  }, {
    // Error case: Missing filename (not sure this will ever happen).
    {kType, kStatus, L"", L"", L"", L"", L"1.0", L"", kAction},
    {kType, kStatus, L"", L"", L"", L"", L"1.0", L"", kAction},
  },
};

TEST_F(EnumerateModulesTest, NormalizeEntry) {
  for (size_t i = 0; i < arraysize(kNormalizationTestCases); ++i) {
    ModuleEnumerator::Module test = kNormalizationTestCases[i].test_case;
    EXPECT_FALSE(test.normalized);
    ModuleEnumerator::NormalizeModule(&test);
    ModuleEnumerator::Module expected = kNormalizationTestCases[i].expected;

    SCOPED_TRACE("Test case no: " + base::IntToString(i));
    EXPECT_EQ(expected.type, test.type);
    EXPECT_EQ(expected.status, test.status);
    EXPECT_STREQ(expected.location.c_str(), test.location.c_str());
    EXPECT_STREQ(expected.name.c_str(), test.name.c_str());
    EXPECT_STREQ(expected.product_name.c_str(), test.product_name.c_str());
    EXPECT_STREQ(expected.description.c_str(), test.description.c_str());
    EXPECT_STREQ(expected.version.c_str(), test.version.c_str());
    EXPECT_STREQ(expected.digital_signer.c_str(), test.digital_signer.c_str());
    EXPECT_EQ(expected.recommended_action, test.recommended_action);
    EXPECT_TRUE(test.normalized);
  }
}

const ModuleEnumerator::Module kStandardModule =
  { kType, kStatus, L"c:\\foo\\bar.dll", L"", L"Prod", L"Desc", L"1.0", L"Sig",
    ModuleEnumerator::NONE };

// Name, location, description and signature are compared by hashing.
static const char kMatchName[] = "88e8c9e0";             // "bar.dll".
static const char kNoMatchName[] = "barfoo.dll";
static const char kMatchLocation[] = "e6ca7b1c";         // "c:\\foo\\".
static const char kNoMatchLocation[] = "c:\\foobar\\";
static const char kMatchDesc[] = "5c4419a6";             // "Desc".
static const char kNoMatchDesc[] = "NoDesc";
static const char kVersionHigh[] = "2.0";
static const char kVersionLow[] = "0.5";
static const char kMatchSignature[] = "7bfd87e1";        // "Sig".
static const char kNoMatchSignature[] = "giS";
static const char kEmpty[] = "";

const struct MatchingEntryList {
  ModuleEnumerator::ModuleStatus expected_result;
  ModuleEnumerator::Module test_case;
  ModuleEnumerator::BlacklistEntry blacklist;
} kMatchineEntryList[] = {
  // Each BlacklistEntry is:
  // Filename, location, desc_or_signer, version from, version to, help_tip.

  {  // Matches: Name (location doesn't match) => Not enough for a match.
    ModuleEnumerator::NOT_MATCHED,
    kStandardModule,
    { kMatchName, kNoMatchLocation, kEmpty, kEmpty, kEmpty,
      ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name (location not given) => Suspected match.
    ModuleEnumerator::SUSPECTED_BAD,
    kStandardModule,
    { kMatchName, kEmpty, kEmpty, kEmpty, kEmpty,
      ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, not version (location not given) => Not a match.
    ModuleEnumerator::NOT_MATCHED,
    kStandardModule,
    { kMatchName, kEmpty, kEmpty, kVersionHigh, kVersionHigh,
      ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location => Suspected match.
    ModuleEnumerator::SUSPECTED_BAD,
    kStandardModule,
    { kMatchName, kMatchLocation, kEmpty, kEmpty, kEmpty,
      ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location (not version) => Not a match.
    ModuleEnumerator::NOT_MATCHED,
    kStandardModule,
    { kMatchName, kMatchLocation, kEmpty, kVersionHigh, kVersionLow,
      ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location, signature => Confirmed match.
    ModuleEnumerator::CONFIRMED_BAD,
    kStandardModule,
    { kMatchName, kMatchLocation, kMatchSignature, kEmpty, kEmpty,
      ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location, signature (not version) => No match.
    ModuleEnumerator::NOT_MATCHED,
    kStandardModule,
    { kMatchName, kMatchLocation, kMatchSignature,
      kVersionLow, kVersionLow, ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location, description => Confirmed match.
    ModuleEnumerator::CONFIRMED_BAD,
    kStandardModule,
    { kMatchName, kMatchLocation, kMatchDesc, kEmpty, kEmpty,
      ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location, description (not version) => No match.
    ModuleEnumerator::NOT_MATCHED,
    kStandardModule,
    { kMatchName, kMatchLocation, kMatchDesc,
      kVersionHigh, kVersionHigh, ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location, signature, version => Confirmed match.
    ModuleEnumerator::CONFIRMED_BAD,
    kStandardModule,
    { kMatchName, kMatchLocation, kMatchSignature,
      kVersionLow, kVersionHigh, ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location, signature, version (lower) => Confirmed.
    ModuleEnumerator::CONFIRMED_BAD,
    kStandardModule,
    { kMatchName, kMatchLocation, kMatchSignature,
      kVersionLow, kEmpty, ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location, signature, version (upper) => Confirmed.
    ModuleEnumerator::CONFIRMED_BAD,
    kStandardModule,
    { kMatchName, kMatchLocation, kMatchSignature,
      kEmpty, kVersionHigh, ModuleEnumerator::SEE_LINK }
  }, {  // All empty fields doesn't produce a match.
    ModuleEnumerator::NOT_MATCHED,
    {kType, kStatus, L"", L"", L"", L"", L""},
    { "a.dll", "", "", "", "", ModuleEnumerator::SEE_LINK }
  },
};

TEST_F(EnumerateModulesTest, MatchFunction) {
  for (size_t i = 0; i < arraysize(kMatchineEntryList); ++i) {
    ModuleEnumerator::Module test = kMatchineEntryList[i].test_case;
    ModuleEnumerator::NormalizeModule(&test);
    ModuleEnumerator::BlacklistEntry blacklist =
        kMatchineEntryList[i].blacklist;

    SCOPED_TRACE("Test case no " + base::IntToString(i) +
                 ": '" + UTF16ToASCII(test.name) + "'");
    EXPECT_EQ(kMatchineEntryList[i].expected_result,
              ModuleEnumerator::Match(test, blacklist));
  }
}
