// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/scoped_temp_dir.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using chrome::spellcheck_common::WordList;

static ProfileKeyedService* BuildSpellcheckService(Profile* profile) {
  return new SpellcheckService(profile);
}

class SpellcheckServiceTest : public testing::Test {
 protected:
  SpellcheckServiceTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        profile_(new TestingProfile()) {
  }

  void SetUp() OVERRIDE {
    // Use SetTestingFactoryAndUse to force creation and initialization.
    SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile_.get(), &BuildSpellcheckService);
  }

  void TearDown() OVERRIDE {
    MessageLoop::current()->RunUntilIdle();
  }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  scoped_ptr<TestingProfile> profile_;
};

// TODO(rlp/rouslan): Shift some of these to a cutsom dictionary test suite.
TEST_F(SpellcheckServiceTest, SpellcheckSetCustomWordList) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());

  WordList loaded_custom_words;
  loaded_custom_words.push_back("foo");
  loaded_custom_words.push_back("bar");
  WordList expected(loaded_custom_words);
  spellcheck_service->GetCustomDictionary()->SetCustomWordList(
      &loaded_custom_words);
  EXPECT_EQ(spellcheck_service->GetCustomWords(), expected);
}

TEST_F(SpellcheckServiceTest, CustomWordAddedLocally) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());

  WordList loaded_custom_words;
  spellcheck_service->GetCustomDictionary()->Load();
  WordList expected;
  EXPECT_EQ(spellcheck_service->GetCustomWords(), expected);
  spellcheck_service->CustomWordAddedLocally("foo");
  expected.push_back("foo");
  EXPECT_EQ(spellcheck_service->GetCustomWords(), expected);
  spellcheck_service->CustomWordAddedLocally("bar");
  expected.push_back("bar");
  EXPECT_EQ(spellcheck_service->GetCustomWords(), expected);
}

TEST_F(SpellcheckServiceTest, SaveAndLoad) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());

  WordList loaded_custom_words;
  spellcheck_service->LoadDictionaryIntoCustomWordList(&loaded_custom_words);

  // The custom word list should be empty now.
  WordList expected;
  EXPECT_EQ(loaded_custom_words, expected);

  spellcheck_service->WriteWordToCustomDictionary("foo");
  expected.push_back("foo");

  spellcheck_service->WriteWordToCustomDictionary("bar");
  expected.push_back("bar");

  // The custom word list should include written words.
  spellcheck_service->LoadDictionaryIntoCustomWordList(&loaded_custom_words);
  EXPECT_EQ(loaded_custom_words, expected);

  // Load in another instance of SpellCheckService.
  // The result should be the same.
  SpellcheckService spellcheck_service2(profile_.get());
  WordList loaded_custom_words2;
  spellcheck_service2.LoadDictionaryIntoCustomWordList(&loaded_custom_words2);
  EXPECT_EQ(loaded_custom_words2, expected);
  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckServiceTest, MultiProfile) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile2, &BuildSpellcheckService));

  WordList expected1;
  WordList expected2;

  spellcheck_service->WriteWordToCustomDictionary("foo");
  spellcheck_service->WriteWordToCustomDictionary("bar");
  expected1.push_back("foo");
  expected1.push_back("bar");

  spellcheck_service2->WriteWordToCustomDictionary("hoge");
  spellcheck_service2->WriteWordToCustomDictionary("fuga");
  expected2.push_back("hoge");
  expected2.push_back("fuga");

  WordList actual1;
  spellcheck_service->LoadDictionaryIntoCustomWordList(&actual1);
  EXPECT_EQ(actual1, expected1);

  WordList actual2;
  spellcheck_service2->LoadDictionaryIntoCustomWordList(&actual2);
  EXPECT_EQ(actual2, expected2);

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckServiceTest, GetSpellCheckLanguages1) {
  std::vector<std::string> accept_languages;
  accept_languages.push_back("en");
  accept_languages.push_back("en-US");
  std::vector<std::string> languages;

  SpellcheckService::GetSpellCheckLanguagesFromAcceptLanguages(
      accept_languages, "en-US", &languages);

  EXPECT_EQ(1U, languages.size());
  EXPECT_EQ("en-US", languages[0]);
}

TEST_F(SpellcheckServiceTest, GetSpellCheckLanguages2) {
  std::vector<std::string> accept_languages;
  accept_languages.push_back("en-US");
  accept_languages.push_back("en");
  std::vector<std::string> languages;

  SpellcheckService::GetSpellCheckLanguagesFromAcceptLanguages(
      accept_languages, "en-US", &languages);

  EXPECT_EQ(1U, languages.size());
  EXPECT_EQ("en-US", languages[0]);
}

TEST_F(SpellcheckServiceTest, GetSpellCheckLanguages3) {
  std::vector<std::string> accept_languages;
  accept_languages.push_back("en");
  accept_languages.push_back("en-US");
  accept_languages.push_back("en-AU");
  std::vector<std::string> languages;

  SpellcheckService::GetSpellCheckLanguagesFromAcceptLanguages(
      accept_languages, "en-US", &languages);

  EXPECT_EQ(2U, languages.size());

  std::sort(languages.begin(), languages.end());
  EXPECT_EQ("en-AU", languages[0]);
  EXPECT_EQ("en-US", languages[1]);
}

TEST_F(SpellcheckServiceTest, GetSpellCheckLanguages4) {
  std::vector<std::string> accept_languages;
  accept_languages.push_back("en");
  accept_languages.push_back("en-US");
  accept_languages.push_back("fr");
  std::vector<std::string> languages;

  SpellcheckService::GetSpellCheckLanguagesFromAcceptLanguages(
      accept_languages, "en-US", &languages);

  EXPECT_EQ(2U, languages.size());

  std::sort(languages.begin(), languages.end());
  EXPECT_EQ("en-US", languages[0]);
  EXPECT_EQ("fr", languages[1]);
}

TEST_F(SpellcheckServiceTest, GetSpellCheckLanguages5) {
  std::vector<std::string> accept_languages;
  accept_languages.push_back("en");
  accept_languages.push_back("en-JP");  // Will not exist.
  accept_languages.push_back("fr");
  accept_languages.push_back("aa");  // Will not exist.
  std::vector<std::string> languages;

  SpellcheckService::GetSpellCheckLanguagesFromAcceptLanguages(
      accept_languages, "fr", &languages);

  EXPECT_EQ(1U, languages.size());
  EXPECT_EQ("fr", languages[0]);
}
