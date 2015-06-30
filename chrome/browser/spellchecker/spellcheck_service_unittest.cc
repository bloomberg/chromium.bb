// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_service.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_split.h"
#include "chrome/browser/spellchecker/feedback_sender.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

struct SpellcheckLanguageTestCase {
  SpellcheckLanguageTestCase(const std::string& unsplit_accept_languages,
                             const std::string& spellcheck_dictionary,
                             const std::string& unsplit_expected_languages)
      : spellcheck_dictionary(spellcheck_dictionary) {
    base::SplitString(unsplit_accept_languages, ',', &accept_languages);
    base::SplitString(unsplit_expected_languages, ',',
                      &expected_spellcheck_languages);
  }
  ~SpellcheckLanguageTestCase() {}

  std::vector<std::string> accept_languages;
  std::string spellcheck_dictionary;
  std::vector<std::string> expected_spellcheck_languages;
};

static scoped_ptr<KeyedService> BuildSpellcheckService(
    content::BrowserContext* profile) {
  return make_scoped_ptr(new SpellcheckService(static_cast<Profile*>(profile)));
}

class SpellcheckServiceUnitTest
    : public testing::TestWithParam<SpellcheckLanguageTestCase> {
 protected:
  void SetUp() override {
    // Use SetTestingFactoryAndUse to force creation and initialization.
    SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        &profile_, &BuildSpellcheckService);
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
};

INSTANTIATE_TEST_CASE_P(
    SpellcheckLanguageTestCases,
    SpellcheckServiceUnitTest,
    testing::Values(
        SpellcheckLanguageTestCase("en,en-US", "en-US", "en-US"),
        SpellcheckLanguageTestCase("en-US,en", "en-US", "en-US"),
        SpellcheckLanguageTestCase("en,en-US,en-AU", "en-US", "en-US,en-AU"),
        SpellcheckLanguageTestCase("en,en-US,fr", "en-US", "en-US,fr"),
        SpellcheckLanguageTestCase("en,en-JP,fr,aa", "fr", "fr")));

TEST_P(SpellcheckServiceUnitTest, GetSpellcheckLanguages) {
  std::vector<std::string> languages;

  SpellcheckService::GetSpellCheckLanguagesFromAcceptLanguages(
      GetParam().accept_languages, GetParam().spellcheck_dictionary,
      &languages);

  EXPECT_EQ(GetParam().expected_spellcheck_languages, languages);
}
