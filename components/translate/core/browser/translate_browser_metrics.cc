// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_browser_metrics.h"

#include <stddef.h>

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "components/language_usage_metrics/language_usage_metrics.h"

namespace translate {

namespace {

// Constant string values to indicate UMA names. All entries should have
// a corresponding index in MetricsNameIndex and an entry in |kMetricsEntries|.
const char kTranslateInitiationStatus[] = "Translate.InitiationStatus.v2";
const char kTranslateReportLanguageDetectionError[] =
    "Translate.ReportLanguageDetectionError";
const char kTranslateLanguageDetectionContentLength[] =
    "Translate.LanguageDetection.ContentLength";
const char kTranslateLocalesOnDisabledByPrefs[] =
    "Translate.LocalesOnDisabledByPrefs";
const char kTranslateUndisplayableLanguage[] =
    "Translate.UndisplayableLanguage";
const char kTranslateUnsupportedLanguageAtInitiation[] =
    "Translate.UnsupportedLanguageAtInitiation";
const char kTranslateSourceLanguage[] = "Translate.SourceLanguage";
const char kTranslateTargetLanguage[] = "Translate.TargetLanguage";
const char kTranslateHrefHintStatus[] = "Translate.HrefHint.Status";
const char kTranslateTargetLanguageOrigin[] = "Translate.TargetLanguage.Origin";

struct MetricsEntry {
  TranslateBrowserMetrics::MetricsNameIndex index;
  const char* const name;
};

// This entry table should be updated when new UMA items are added. This is only
// used by GetMetricsName for testing.
const MetricsEntry kMetricsEntries[] = {
    {TranslateBrowserMetrics::UMA_INITIATION_STATUS,
     kTranslateInitiationStatus},
    {TranslateBrowserMetrics::UMA_LANGUAGE_DETECTION_ERROR,
     kTranslateReportLanguageDetectionError},
    {TranslateBrowserMetrics::UMA_LOCALES_ON_DISABLED_BY_PREFS,
     kTranslateLocalesOnDisabledByPrefs},
    {TranslateBrowserMetrics::UMA_UNDISPLAYABLE_LANGUAGE,
     kTranslateUndisplayableLanguage},
    {TranslateBrowserMetrics::UMA_UNSUPPORTED_LANGUAGE_AT_INITIATION,
     kTranslateUnsupportedLanguageAtInitiation},
    {TranslateBrowserMetrics::UMA_TRANSLATE_SOURCE_LANGUAGE,
     kTranslateSourceLanguage},
    {TranslateBrowserMetrics::UMA_TRANSLATE_TARGET_LANGUAGE,
     kTranslateTargetLanguage},
    {TranslateBrowserMetrics::UMA_TRANSLATE_HREF_HINT_STATUS,
     kTranslateHrefHintStatus},
    {TranslateBrowserMetrics::UMA_TRANSLATE_TARGET_LANGUAGE_ORIGIN,
     kTranslateTargetLanguageOrigin},
};

static_assert(base::size(kMetricsEntries) == TranslateBrowserMetrics::UMA_MAX,
              "kMetricsEntries should have UMA_MAX elements");

}  // namespace

namespace TranslateBrowserMetrics {

void ReportInitiationStatus(InitiationStatusType type) {
  UMA_HISTOGRAM_ENUMERATION(kTranslateInitiationStatus, type,
                            INITIATION_STATUS_MAX);
}

void ReportLanguageDetectionError() {
  UMA_HISTOGRAM_BOOLEAN(kTranslateReportLanguageDetectionError, true);
}

void ReportLanguageDetectionContentLength(size_t length) {
  base::UmaHistogramCounts100000(kTranslateLanguageDetectionContentLength,
                                 length);
}

void ReportLocalesOnDisabledByPrefs(const std::string& locale) {
  base::UmaHistogramSparse(
      kTranslateLocalesOnDisabledByPrefs,
      language_usage_metrics::LanguageUsageMetrics::ToLanguageCode(locale));
}

void ReportUndisplayableLanguage(const std::string& language) {
  int language_code =
      language_usage_metrics::LanguageUsageMetrics::ToLanguageCode(language);
  base::UmaHistogramSparse(kTranslateUndisplayableLanguage, language_code);
}

void ReportUnsupportedLanguageAtInitiation(const std::string& language) {
  int language_code =
      language_usage_metrics::LanguageUsageMetrics::ToLanguageCode(language);
  base::UmaHistogramSparse(kTranslateUnsupportedLanguageAtInitiation,
                           language_code);
}

void ReportTranslateSourceLanguage(const std::string& language) {
  base::UmaHistogramSparse(kTranslateSourceLanguage,
                           base::HashMetricName(language));
}

void ReportTranslateTargetLanguage(const std::string& language) {
  base::UmaHistogramSparse(kTranslateTargetLanguage,
                           base::HashMetricName(language));
}

void ReportTranslateHrefHintStatus(HrefTranslateStatus status) {
  base::UmaHistogramEnumeration(kTranslateHrefHintStatus, status);
}

void ReportTranslateTargetLanguageOrigin(TargetLanguageOrigin origin) {
  base::UmaHistogramEnumeration(kTranslateTargetLanguageOrigin, origin);
}

const char* GetMetricsName(MetricsNameIndex index) {
  for (size_t i = 0; i < base::size(kMetricsEntries); ++i) {
    if (kMetricsEntries[i].index == index)
      return kMetricsEntries[i].name;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace TranslateBrowserMetrics

}  // namespace translate
