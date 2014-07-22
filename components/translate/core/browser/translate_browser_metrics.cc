// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_browser_metrics.h"

#include <string>

#include "base/basictypes.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "components/language_usage_metrics/language_usage_metrics.h"

namespace translate {

namespace {

// Constant string values to indicate UMA names. All entries should have
// a corresponding index in MetricsNameIndex and an entry in |kMetricsEntries|.
const char kTranslateInitiationStatus[] =
    "Translate.InitiationStatus.v2";
const char kTranslateReportLanguageDetectionError[] =
    "Translate.ReportLanguageDetectionError";
const char kTranslateLocalesOnDisabledByPrefs[] =
    "Translate.LocalesOnDisabledByPrefs";
const char kTranslateUndisplayableLanguage[] =
    "Translate.UndisplayableLanguage";
const char kTranslateUnsupportedLanguageAtInitiation[] =
    "Translate.UnsupportedLanguageAtInitiation";

struct MetricsEntry {
  TranslateBrowserMetrics::MetricsNameIndex index;
  const char* const name;
};

// This entry table should be updated when new UMA items are added.
const MetricsEntry kMetricsEntries[] = {
  { TranslateBrowserMetrics::UMA_INITIATION_STATUS,
    kTranslateInitiationStatus },
  { TranslateBrowserMetrics::UMA_LANGUAGE_DETECTION_ERROR,
    kTranslateReportLanguageDetectionError },
  { TranslateBrowserMetrics::UMA_LOCALES_ON_DISABLED_BY_PREFS,
    kTranslateLocalesOnDisabledByPrefs },
  { TranslateBrowserMetrics::UMA_UNDISPLAYABLE_LANGUAGE,
    kTranslateUndisplayableLanguage },
  { TranslateBrowserMetrics::UMA_UNSUPPORTED_LANGUAGE_AT_INITIATION,
    kTranslateUnsupportedLanguageAtInitiation },
};

COMPILE_ASSERT(arraysize(kMetricsEntries) == TranslateBrowserMetrics::UMA_MAX,
               arraysize_of_kMetricsEntries_should_be_UMA_MAX);

}  // namespace

namespace TranslateBrowserMetrics {

void ReportInitiationStatus(InitiationStatusType type) {
  UMA_HISTOGRAM_ENUMERATION(kTranslateInitiationStatus,
                            type,
                            INITIATION_STATUS_MAX);
}

void ReportLanguageDetectionError() {
  UMA_HISTOGRAM_BOOLEAN(kTranslateReportLanguageDetectionError, true);
}

void ReportLocalesOnDisabledByPrefs(const std::string& locale) {
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      kTranslateLocalesOnDisabledByPrefs,
      language_usage_metrics::LanguageUsageMetrics::ToLanguageCode(locale));
}

void ReportUndisplayableLanguage(const std::string& language) {
  int language_code =
      language_usage_metrics::LanguageUsageMetrics::ToLanguageCode(language);
  UMA_HISTOGRAM_SPARSE_SLOWLY(kTranslateUndisplayableLanguage,
                              language_code);
}

void ReportUnsupportedLanguageAtInitiation(const std::string& language) {
  int language_code =
      language_usage_metrics::LanguageUsageMetrics::ToLanguageCode(language);
  UMA_HISTOGRAM_SPARSE_SLOWLY(kTranslateUnsupportedLanguageAtInitiation,
                              language_code);
}

const char* GetMetricsName(MetricsNameIndex index) {
  for (size_t i = 0; i < arraysize(kMetricsEntries); ++i) {
    if (kMetricsEntries[i].index == index)
      return kMetricsEntries[i].name;
  }
  NOTREACHED();
  return NULL;
}

}  // namespace TranslateBrowserMetrics

}  // namespace translate
