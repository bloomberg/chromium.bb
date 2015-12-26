// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_service.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_split.h"
#include "base/supports_user_data.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

struct SpellcheckLanguageTestCase {
  SpellcheckLanguageTestCase(const std::string& accept_languages,
                             const std::string& unsplit_spellcheck_dictionaries,
                             size_t num_expected_enabled_spellcheck_languages,
                             const std::string& unsplit_expected_languages)
      : accept_languages(accept_languages),
        num_expected_enabled_spellcheck_languages(
            num_expected_enabled_spellcheck_languages) {
    spellcheck_dictionaries = base::SplitString(
        unsplit_spellcheck_dictionaries, ",",
        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    expected_spellcheck_languages = base::SplitString(
        unsplit_expected_languages, ",",
        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  }
  ~SpellcheckLanguageTestCase() {}

  std::string accept_languages;
  std::vector<std::string> spellcheck_dictionaries;
  size_t num_expected_enabled_spellcheck_languages;
  std::vector<std::string> expected_spellcheck_languages;
};

class SpellcheckServiceUnitTest
    : public testing::TestWithParam<SpellcheckLanguageTestCase> {
 public:
  SpellcheckServiceUnitTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
    user_prefs::UserPrefs::Set(&context_, &prefs_);
  }
  ~SpellcheckServiceUnitTest() override {}

  void SetUp() override {
    prefs()->registry()->RegisterListPref(prefs::kSpellCheckDictionaries);
    prefs()->registry()->RegisterStringPref(prefs::kAcceptLanguages,
                                            std::string());
  }

  base::SupportsUserData* context() { return &context_; }
  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  struct : public base::SupportsUserData {} context_;
  TestingPrefServiceSimple prefs_;
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckServiceUnitTest);
};

INSTANTIATE_TEST_CASE_P(
    SpellcheckLanguageTestCases,
    SpellcheckServiceUnitTest,
    testing::Values(
        SpellcheckLanguageTestCase("en,en-US", "en-US", 1UL, "en-US"),
        SpellcheckLanguageTestCase("en-US,en", "en-US", 1UL, "en-US"),
        SpellcheckLanguageTestCase("en,en-US,en-AU",
                                   "en-US",
                                   1UL,
                                   "en-US,en-AU"),
        SpellcheckLanguageTestCase("en,en-US,fr", "en-US", 1UL, "en-US,fr"),
        SpellcheckLanguageTestCase("en,en-JP,fr,aa", "fr", 1UL, "fr"),
        SpellcheckLanguageTestCase("en,en-US", "en-US", 1UL, "en-US"),
        SpellcheckLanguageTestCase("en-US,en", "en-US", 1UL, "en-US"),
        SpellcheckLanguageTestCase("en,fr,en-US,en-AU",
                                   "en-US,fr",
                                   2UL,
                                   "en-US,fr,en-AU"),
        SpellcheckLanguageTestCase("en,en-JP,fr,zz,en-US",
                                   "fr",
                                   1UL,
                                   "fr,en-US")));

TEST_P(SpellcheckServiceUnitTest, GetSpellcheckLanguages) {
  prefs()->SetString(prefs::kAcceptLanguages, GetParam().accept_languages);
  base::ListValue dictionaries;
  dictionaries.AppendStrings(GetParam().spellcheck_dictionaries);
  prefs()->Set(prefs::kSpellCheckDictionaries, dictionaries);

  std::vector<std::string> spellcheck_languages;
  EXPECT_EQ(GetParam().num_expected_enabled_spellcheck_languages,
            SpellcheckService::GetSpellCheckLanguages(context(),
                                                      &spellcheck_languages));

  EXPECT_EQ(GetParam().expected_spellcheck_languages, spellcheck_languages);
}
