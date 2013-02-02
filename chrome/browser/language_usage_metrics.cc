// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language_usage_metrics.h"

#include <algorithm>

#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/strings/string_tokenizer.h"

namespace {
void RecordAcceptLanguage(Language language) {
  UMA_HISTOGRAM_ENUMERATION("LanguageUsageMetrics.AcceptLanguage",
                            language, NUM_LANGUAGES);
}
}  // namespace

// static
void LanguageUsageMetrics::RecordAcceptLanguages(
    const std::string& accept_languages) {
  // Rethink about the histogram memory costs when the number of languages
  // becomes too large.
  DCHECK_LE(NUM_LANGUAGES, 300);

  std::set<Language> languages;
  ParseAcceptLanguages(accept_languages, &languages);
  std::for_each(languages.begin(), languages.end(), RecordAcceptLanguage);
}

// static
void LanguageUsageMetrics::RecordApplicationLanguage(
    const std::string& application_locale) {
  const Language language = ToLanguage(application_locale);
  if (language != UNKNOWN_LANGUAGE) {
    UMA_HISTOGRAM_ENUMERATION("LanguageUsageMetrics.ApplicationLanguage",
                              language, NUM_LANGUAGES);
  }
}

// static
void LanguageUsageMetrics::ParseAcceptLanguages(
    const std::string& accept_languages, std::set<Language>* languages) {
  languages->clear();
  base::StringTokenizer locales(accept_languages, ",");
  while (locales.GetNext()) {
    const Language language = ToLanguage(locales.token());
    if (language != UNKNOWN_LANGUAGE) {
      languages->insert(language);
    }
  }
}

// static
Language LanguageUsageMetrics::ToLanguage(const std::string& locale) {
  base::StringTokenizer parts(locale, "-_");
  if (!parts.GetNext()) {
    return UNKNOWN_LANGUAGE;
  }
  const std::string language_part = parts.token();
  Language language;
  if (!LanguageFromCode(language_part.c_str(), &language)) {
    return UNKNOWN_LANGUAGE;
  }
  return language;
}
