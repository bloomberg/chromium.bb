// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
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

class SpellcheckServiceTest : public testing::Test {
 protected:
  SpellcheckServiceTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        profile_(new TestingProfile()) {
  }

  virtual void SetUp() OVERRIDE {
    // Use SetTestingFactoryAndUse to force creation and initialization.
    SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile_.get(), &BuildSpellcheckService);
  }

  virtual void TearDown() OVERRIDE {
    MessageLoop::current()->RunUntilIdle();
  }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  scoped_ptr<TestingProfile> profile_;
};

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
