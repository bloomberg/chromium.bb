// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/enumerate_modules_model_win.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test EnumerateModulesTest;

// Set up some constants to use as default when creating the structs.
static const ModuleEnumerator::ModuleType kType =
    ModuleEnumerator::LOADED_MODULE;

static const ModuleEnumerator::ModuleStatus kStatus =
    ModuleEnumerator::NOT_MATCHED;

static const ModuleEnumerator::RecommendedAction kAction =
    ModuleEnumerator::NONE;

static const ModuleEnumerator::OperatingSystem kOs =
    ModuleEnumerator::ALL;

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
const ModuleEnumerator::Module kStandardModuleNoDescription =
  { kType, kStatus, L"c:\\foo\\bar.dll", L"", L"Prod", L"", L"1.0", L"Sig",
    ModuleEnumerator::NONE };
const ModuleEnumerator::Module kStandardModuleNoSignature =
  { kType, kStatus, L"c:\\foo\\bar.dll", L"", L"Prod", L"Desc", L"1.0", L"",
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
    { kMatchName, kNoMatchLocation, kEmpty, kEmpty, kEmpty, kOs,
      ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name (location not given) => Suspected match.
    ModuleEnumerator::SUSPECTED_BAD,
    kStandardModule,
    { kMatchName, kEmpty, kEmpty, kEmpty, kEmpty, kOs,
      ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, not version (location not given) => Not a match.
    ModuleEnumerator::NOT_MATCHED,
    kStandardModule,
    { kMatchName, kEmpty, kEmpty, kVersionHigh, kVersionHigh, kOs,
      ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location => Suspected match.
    ModuleEnumerator::SUSPECTED_BAD,
    kStandardModule,
    { kMatchName, kMatchLocation, kEmpty, kEmpty, kEmpty, kOs,
      ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location, (description not given) => Confirmed match.
    ModuleEnumerator::CONFIRMED_BAD,
    kStandardModuleNoDescription,  // Note: No description.
    { kMatchName, kMatchLocation, kEmpty, kEmpty, kEmpty, kOs,
      ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location, (signature not given) => Confirmed match.
    ModuleEnumerator::CONFIRMED_BAD,
    kStandardModuleNoSignature,  // Note: No signature.
    { kMatchName, kMatchLocation, kEmpty, kEmpty, kEmpty, kOs,
      ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location (not version) => Not a match.
    ModuleEnumerator::NOT_MATCHED,
    kStandardModule,
    { kMatchName, kMatchLocation, kEmpty, kVersionHigh, kVersionLow, kOs,
      ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location, signature => Confirmed match.
    ModuleEnumerator::CONFIRMED_BAD,
    kStandardModule,
    { kMatchName, kMatchLocation, kMatchSignature, kEmpty, kEmpty, kOs,
      ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location, signature (not version) => No match.
    ModuleEnumerator::NOT_MATCHED,
    kStandardModule,
    { kMatchName, kMatchLocation, kMatchSignature,
      kVersionLow, kVersionLow, kOs, ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location, description => Confirmed match.
    ModuleEnumerator::CONFIRMED_BAD,
    kStandardModule,
    { kMatchName, kMatchLocation, kMatchDesc, kEmpty, kEmpty, kOs,
      ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location, description (not version) => No match.
    ModuleEnumerator::NOT_MATCHED,
    kStandardModule,
    { kMatchName, kMatchLocation, kMatchDesc,
      kVersionHigh, kVersionHigh, kOs, ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location, signature, version => Confirmed match.
    ModuleEnumerator::CONFIRMED_BAD,
    kStandardModule,
    { kMatchName, kMatchLocation, kMatchSignature,
      kVersionLow, kVersionHigh, kOs, ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location, signature, version (lower) => Confirmed.
    ModuleEnumerator::CONFIRMED_BAD,
    kStandardModule,
    { kMatchName, kMatchLocation, kMatchSignature,
      kVersionLow, kEmpty, kOs, ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, location, signature, version (upper) => Confirmed.
    ModuleEnumerator::CONFIRMED_BAD,
    kStandardModule,
    { kMatchName, kMatchLocation, kMatchSignature,
      kEmpty, kVersionHigh, kOs, ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, Location, Version lower is inclusive => Confirmed.
    ModuleEnumerator::CONFIRMED_BAD,
    kStandardModule,
    { kMatchName, kMatchLocation, kMatchSignature,
      "1.0", "2.0", kOs, ModuleEnumerator::SEE_LINK }
  }, {  // Matches: Name, Location, Version higher is exclusive => No match.
    ModuleEnumerator::NOT_MATCHED,
    kStandardModule,
    { kMatchName, kMatchLocation, kEmpty,
      "0.0", "1.0", kOs, ModuleEnumerator::SEE_LINK }
  }, {  // All empty fields doesn't produce a match.
    ModuleEnumerator::NOT_MATCHED,
    { kType, kStatus, L"", L"", L"", L"", L""},
    { "a.dll", "", "", "", "", kOs, ModuleEnumerator::SEE_LINK }
  },
};

TEST_F(EnumerateModulesTest, MatchFunction) {
  for (size_t i = 0; i < arraysize(kMatchineEntryList); ++i) {
    ModuleEnumerator::Module test = kMatchineEntryList[i].test_case;
    ModuleEnumerator::NormalizeModule(&test);
    ModuleEnumerator::BlacklistEntry blacklist =
        kMatchineEntryList[i].blacklist;

    SCOPED_TRACE("Test case no " + base::IntToString(i) +
                 ": '" + base::UTF16ToASCII(test.name) + "'");
    EXPECT_EQ(kMatchineEntryList[i].expected_result,
              ModuleEnumerator::Match(test, blacklist));
  }
}

const struct CollapsePathList {
  base::string16 expected_result;
  base::string16 test_case;
} kCollapsePathList[] = {
  // Negative testing (should not collapse this path).
  { base::ASCIIToUTF16("c:\\a\\a.dll"), base::ASCIIToUTF16("c:\\a\\a.dll") },
  // These two are to test that we select the maximum collapsed path.
  { base::ASCIIToUTF16("%foo%\\a.dll"), base::ASCIIToUTF16("c:\\foo\\a.dll") },
  { base::ASCIIToUTF16("%x%\\a.dll"),
    base::ASCIIToUTF16("c:\\foo\\bar\\a.dll") },
};

TEST_F(EnumerateModulesTest, CollapsePath) {
  scoped_refptr<ModuleEnumerator> module_enumerator(new ModuleEnumerator(NULL));
  module_enumerator->path_mapping_.clear();
  module_enumerator->path_mapping_.push_back(
      std::make_pair(L"c:\\foo\\", L"%foo%"));
  module_enumerator->path_mapping_.push_back(
      std::make_pair(L"c:\\foo\\bar\\", L"%x%"));

  for (size_t i = 0; i < arraysize(kCollapsePathList); ++i) {
    ModuleEnumerator::Module module;
    module.location = kCollapsePathList[i].test_case;
    module_enumerator->CollapsePath(&module);

    SCOPED_TRACE("Test case no " + base::IntToString(i) + ": '" +
                 base::UTF16ToASCII(kCollapsePathList[i].expected_result) +
                 "'");
    EXPECT_EQ(kCollapsePathList[i].expected_result, module.location);
  }
}
