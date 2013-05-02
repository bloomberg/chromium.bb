// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/translate/translate_helper_metrics.h"

#include "base/metrics/histogram.h"

namespace {

const char kTranslateContentLanguage[] = "Translate.ContentLanguage";
const char kTranslateLanguageVerification[] = "Translate.LanguageVerification";

}  // namespace

namespace TranslateHelperMetrics {

void ReportContentLanguage(const std::string& provided_code,
                           const std::string& revised_code) {
  if (provided_code.empty()) {
    UMA_HISTOGRAM_ENUMERATION(kTranslateContentLanguage,
                              CONTENT_LANGUAGE_NOT_PROVIDED,
                              CONTENT_LANGUAGE_MAX);
  } else if (provided_code == revised_code) {
    UMA_HISTOGRAM_ENUMERATION(kTranslateContentLanguage,
                              CONTENT_LANGUAGE_VALID,
                              CONTENT_LANGUAGE_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kTranslateContentLanguage,
                              CONTENT_LANGUAGE_INVALID,
                              CONTENT_LANGUAGE_MAX);
  }
}

void ReportLanguageVerification(LanguageVerificationType type) {
  UMA_HISTOGRAM_ENUMERATION(kTranslateLanguageVerification,
                            type,
                            LANGUAGE_VERIFICATION_MAX);
}

} // namespace TranslateHelperMetrics
