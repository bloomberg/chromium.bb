// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_language_list.h"

#include "components/translate/core/browser/translate_download_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {

TEST(TranslateLanguageListTest, SetSupportedLanguages) {
  std::string language_list(
      "{"
      "\"sl\":{\"en\":\"English\",\"ja\":\"Japanese\"},"
      "\"tl\":{\"en\":\"English\",\"ja\":\"Japanese\"}"
      "}");
  TranslateDownloadManager* manager = TranslateDownloadManager::GetInstance();
  manager->set_application_locale("en");
  EXPECT_TRUE(manager->language_list()->SetSupportedLanguages(language_list));

  std::vector<std::string> results;
  manager->language_list()->GetSupportedLanguages(&results);
  ASSERT_EQ(2u, results.size());
  EXPECT_EQ("en", results[0]);
  EXPECT_EQ("ja", results[1]);
}

TEST(TranslateLanguageListTest, SetSupportedLanguagesWithAlphaKey) {
  std::string language_list(
      "{"
      "\"sl\":{\"en\":\"English\",\"ja\":\"Japanese\"},"
      "\"tl\":{\"en\":\"English\",\"ja\":\"Japanese\"},"
      "\"al\":{\"en\":1}"
      "}");
  TranslateDownloadManager* manager = TranslateDownloadManager::GetInstance();
  manager->set_application_locale("en");
  EXPECT_TRUE(manager->language_list()->SetSupportedLanguages(language_list));

  std::vector<std::string> results;
  manager->language_list()->GetSupportedLanguages(&results);
  ASSERT_EQ(2u, results.size());
  EXPECT_EQ("en", results[0]);
  EXPECT_EQ("ja", results[1]);
}

}  // namespace translate
