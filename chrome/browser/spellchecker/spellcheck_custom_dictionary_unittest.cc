// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
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

class SpellcheckCustomDictionaryTest : public testing::Test {
 protected:
  SpellcheckCustomDictionaryTest()
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

TEST_F(SpellcheckCustomDictionaryTest, SpellcheckSetCustomWordList) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());

  WordList loaded_custom_words;
  loaded_custom_words.push_back("foo");
  loaded_custom_words.push_back("bar");
  WordList expected(loaded_custom_words);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  custom_dictionary->SetCustomWordList(&loaded_custom_words);
  EXPECT_EQ(custom_dictionary->GetWords(), expected);
}

TEST_F(SpellcheckCustomDictionaryTest, CustomWordAddedAndRemovedLocally) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());

  WordList loaded_custom_words;
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  WordList expected;
  EXPECT_EQ(custom_dictionary->GetWords(), expected);
  custom_dictionary->CustomWordAddedLocally("foo");
  expected.push_back("foo");
  EXPECT_EQ(custom_dictionary->GetWords(), expected);
  custom_dictionary->CustomWordAddedLocally("bar");
  expected.push_back("bar");
  EXPECT_EQ(custom_dictionary->GetWords(), expected);

  custom_dictionary->CustomWordRemovedLocally("foo");
  custom_dictionary->CustomWordRemovedLocally("bar");
  expected.clear();
  EXPECT_EQ(custom_dictionary->GetWords(), expected);
}

TEST_F(SpellcheckCustomDictionaryTest, SaveAndLoad) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();

  WordList loaded_custom_words;
  custom_dictionary->LoadDictionaryIntoCustomWordList(&loaded_custom_words);

  // The custom word list should be empty now.
  WordList expected;
  EXPECT_EQ(loaded_custom_words, expected);

  custom_dictionary->WriteWordToCustomDictionary("foo");
  expected.push_back("foo");

  custom_dictionary->WriteWordToCustomDictionary("bar");
  expected.push_back("bar");

  // The custom word list should include written words.
  custom_dictionary->LoadDictionaryIntoCustomWordList(&loaded_custom_words);
  std::sort(expected.begin(), expected.end());
  EXPECT_EQ(loaded_custom_words, expected);

  // Load in another instance of SpellCheckService.
  // The result should be the same.
  SpellcheckService spellcheck_service2(profile_.get());
  WordList loaded_custom_words2;
  spellcheck_service2.GetCustomDictionary()->
      LoadDictionaryIntoCustomWordList(&loaded_custom_words2);
  EXPECT_EQ(loaded_custom_words2, expected);

  custom_dictionary->EraseWordFromCustomDictionary("foo");
  custom_dictionary->EraseWordFromCustomDictionary("bar");
  custom_dictionary->LoadDictionaryIntoCustomWordList(&loaded_custom_words);
  expected.clear();
  EXPECT_EQ(loaded_custom_words, expected);

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, MultiProfile) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile2, &BuildSpellcheckService));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  WordList expected1;
  WordList expected2;

  custom_dictionary->WriteWordToCustomDictionary("foo");
  custom_dictionary->WriteWordToCustomDictionary("bar");
  expected1.push_back("foo");
  expected1.push_back("bar");

  custom_dictionary2->WriteWordToCustomDictionary("hoge");
  custom_dictionary2->WriteWordToCustomDictionary("fuga");
  expected2.push_back("hoge");
  expected2.push_back("fuga");

  WordList actual1;
  custom_dictionary->LoadDictionaryIntoCustomWordList(&actual1);
  std::sort(expected1.begin(), expected1.end());
  EXPECT_EQ(actual1, expected1);

  WordList actual2;
  custom_dictionary2->LoadDictionaryIntoCustomWordList(&actual2);
  std::sort(expected2.begin(), expected2.end());
  EXPECT_EQ(actual2, expected2);

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}
