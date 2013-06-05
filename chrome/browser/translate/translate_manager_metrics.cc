// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_manager_metrics.h"

#include <string>

#include "base/basictypes.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "chrome/browser/language_usage_metrics.h"

namespace {

// Constant string values to indicate UMA names. All entries should have
// a corresponding index in MetricsNameIndex and an entry in |kMetricsEntries|.
const char kTranslateInitiationStatus[] =
    "Translate.InitiationStatus";
const char kTranslateReportLanguageDetectionError[] =
    "Translate.ReportLanguageDetectionError";
const char kTranslateServerReportedUnsupportedLanguage[] =
    "Translate.ServerReportedUnsupportedLanguage";
const char kTranslateUnsupportedLanguageAtInitiation[] =
    "Translate.UnsupportedLanguageAtInitiation";
const char kTranslateLocalesOnDisabledByPrefs[] =
    "Translate.LocalesOnDisabledByPrefs";

struct MetricsEntry {
  TranslateManagerMetrics::MetricsNameIndex index;
  const char* const name;
};

// This entry table should be updated when new UMA items are added.
const MetricsEntry kMetricsEntries[] = {
  { TranslateManagerMetrics::UMA_INITIATION_STATUS,
    kTranslateInitiationStatus },
  { TranslateManagerMetrics::UMA_LANGUAGE_DETECTION_ERROR,
    kTranslateReportLanguageDetectionError },
  { TranslateManagerMetrics::UMA_SERVER_REPORTED_UNSUPPORTED_LANGUAGE,
    kTranslateServerReportedUnsupportedLanguage },
  { TranslateManagerMetrics::UMA_UNSUPPORTED_LANGUAGE_AT_INITIATION,
    kTranslateUnsupportedLanguageAtInitiation },
  { TranslateManagerMetrics::UMA_LOCALES_ON_DISABLED_BY_PREFS,
    kTranslateLocalesOnDisabledByPrefs },
};

COMPILE_ASSERT(arraysize(kMetricsEntries) == TranslateManagerMetrics::UMA_MAX,
               arraysize_of_kMetricsEntries_should_be_UMA_MAX);

}  // namespace

namespace TranslateManagerMetrics {

void ReportInitiationStatus(InitiationStatusType type) {
  UMA_HISTOGRAM_ENUMERATION(kTranslateInitiationStatus,
                            type,
                            INITIATION_STATUS_MAX);
}

void ReportLanguageDetectionError() {
  UMA_HISTOGRAM_BOOLEAN(kTranslateReportLanguageDetectionError, true);
}

void ReportUnsupportedLanguage() {
  UMA_HISTOGRAM_BOOLEAN(kTranslateServerReportedUnsupportedLanguage, true);
}

void ReportUnsupportedLanguageAtInitiation(const std::string& language) {
  int language_code = LanguageUsageMetrics::ToLanguageCode(language);
  UMA_HISTOGRAM_SPARSE_SLOWLY(kTranslateUnsupportedLanguageAtInitiation,
                              language_code);
}

void ReportLocalesOnDisabledByPrefs(const std::string& locale) {
  UMA_HISTOGRAM_SPARSE_SLOWLY(kTranslateLocalesOnDisabledByPrefs,
                              LanguageUsageMetrics::ToLanguageCode(locale));
}

const char* GetMetricsName(MetricsNameIndex index) {
  for (size_t i = 0; i < arraysize(kMetricsEntries); ++i) {
    if (kMetricsEntries[i].index == index)
      return kMetricsEntries[i].name;
  }
  NOTREACHED();
  return NULL;
}

}  // namespace TranslateManagerMetrics
