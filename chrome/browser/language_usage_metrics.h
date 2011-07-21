// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LANGUAGE_USAGE_METRICS_H_
#define CHROME_BROWSER_LANGUAGE_USAGE_METRICS_H_
#pragma once

#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "third_party/cld/languages/public/languages.h"

// Methods to record language usage as UMA histograms.
// Language codes are defined in third_party/cld/languages/proto/languages.pb.h
class LanguageUsageMetrics {
 public:
  // Records accept languages as a UMA histogram. |accept_languages| is a
  // case-insensitive comma-separated list of languages/locales of either xx,
  // xx-YY, or xx_YY format where xx is iso-639 language code and YY is iso-3166
  // country code. Country code is ignored. That is, xx and XX-YY are considered
  // identical and recorded once.
  static void RecordAcceptLanguages(const std::string& accept_languages);

  // Records the application language as a UMA histogram. |application_locale|
  // is a case-insensitive locale string of either xx, xx-YY, or xx_YY format.
  // Only the language part (xx in the example) is considered.
  static void RecordApplicationLanguage(const std::string& application_locale);

 private:
  // This class must not be instantiated.
  LanguageUsageMetrics();

  // Parses |accept_languages| and returns a set of language codes in
  // |languages|.
  static void ParseAcceptLanguages(const std::string& accept_languages,
                                   std::set<Language>* languages);

  // Parses |locale| and returns the language code. Returns UNKNOWN_LANGUAGE in
  // case of errors.
  static Language ToLanguage(const std::string& locale);

  FRIEND_TEST_ALL_PREFIXES(LanguageUsageMetricsTest, ParseAcceptLanguages);
  FRIEND_TEST_ALL_PREFIXES(LanguageUsageMetricsTest, ToLanguage);
};

#endif  // CHROME_BROWSER_LANGUAGE_USAGE_METRICS_H_
