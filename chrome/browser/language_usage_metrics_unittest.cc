// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language_usage_metrics.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(LanguageUsageMetricsTest, ParseAcceptLanguages) {
  std::set<Language> language_set;
  std::set<Language>::const_iterator it;

  // Basic single language case.
  LanguageUsageMetrics::ParseAcceptLanguages("ja", &language_set);
  EXPECT_EQ(1U, language_set.size());
  EXPECT_EQ(JAPANESE, *language_set.begin());

  // Empty language.
  LanguageUsageMetrics::ParseAcceptLanguages(std::string(), &language_set);
  EXPECT_EQ(0U, language_set.size());

  // Country code is ignored.
  LanguageUsageMetrics::ParseAcceptLanguages("ja-JP", &language_set);
  EXPECT_EQ(1U, language_set.size());
  EXPECT_EQ(JAPANESE, *language_set.begin());

  // Case is ignored.
  LanguageUsageMetrics::ParseAcceptLanguages("Ja-jP", &language_set);
  EXPECT_EQ(1U, language_set.size());
  EXPECT_EQ(JAPANESE, *language_set.begin());

  // Underscore as the separator.
  LanguageUsageMetrics::ParseAcceptLanguages("ja_JP", &language_set);
  EXPECT_EQ(1U, language_set.size());
  EXPECT_EQ(JAPANESE, *language_set.begin());

  // The result contains a same language code only once.
  LanguageUsageMetrics::ParseAcceptLanguages("ja-JP,ja", &language_set);
  EXPECT_EQ(1U, language_set.size());
  EXPECT_EQ(JAPANESE, *language_set.begin());

  // Basic two languages case.
  LanguageUsageMetrics::ParseAcceptLanguages("en,ja", &language_set);
  EXPECT_EQ(2U, language_set.size());
  it = language_set.begin();
  EXPECT_EQ(ENGLISH, *it);
  EXPECT_EQ(JAPANESE, *++it);

  // Multiple languages.
  LanguageUsageMetrics::ParseAcceptLanguages("ja-JP,en,es,ja,en-US",
                                             &language_set);
  EXPECT_EQ(3U, language_set.size());
  it = language_set.begin();
  EXPECT_EQ(ENGLISH, *it);
  EXPECT_EQ(JAPANESE, *++it);
  EXPECT_EQ(SPANISH, *++it);

  // Two empty languages.
  LanguageUsageMetrics::ParseAcceptLanguages(",", &language_set);
  EXPECT_EQ(0U, language_set.size());

  // Trailing comma.
  LanguageUsageMetrics::ParseAcceptLanguages("ja,", &language_set);
  EXPECT_EQ(1U, language_set.size());
  EXPECT_EQ(JAPANESE, *language_set.begin());

  // Leading comma.
  LanguageUsageMetrics::ParseAcceptLanguages(",es", &language_set);
  EXPECT_EQ(1U, language_set.size());
  EXPECT_EQ(SPANISH, *language_set.begin());

  // Combination of invalid and valid.
  LanguageUsageMetrics::ParseAcceptLanguages("zz,en", &language_set);
  EXPECT_EQ(1U, language_set.size());
  EXPECT_EQ(ENGLISH, *language_set.begin());
}

TEST(LanguageUsageMetricsTest, ToLanguage) {
  // Basic case.
  EXPECT_EQ(JAPANESE, LanguageUsageMetrics::ToLanguage("ja"));

  // Case is ignored.
  EXPECT_EQ(SPANISH, LanguageUsageMetrics::ToLanguage("Es"));

  // Coutry code is ignored.
  EXPECT_EQ(JAPANESE, LanguageUsageMetrics::ToLanguage("ja-JP"));

  // Invalid locales are considered as unknown language.
  EXPECT_EQ(UNKNOWN_LANGUAGE, LanguageUsageMetrics::ToLanguage(std::string()));
  EXPECT_EQ(UNKNOWN_LANGUAGE, LanguageUsageMetrics::ToLanguage("xx"));
}
