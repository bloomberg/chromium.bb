// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/translate/translate_helper_metrics.h"

#include "base/metrics/histogram.h"

namespace {

// Constant string values to indicate UMA names. All entry should have
// a corresponding index in MetricsNameIndex and an entry in |kMetricsEntries|.
const char kRenderer4LanguageDetection[] = "Renderer4.LanguageDetection";
const char kTranslateContentLanguage[] = "Translate.ContentLanguage";
const char kTranslateLanguageVerification[] = "Translate.LanguageVerification";
const char kTranslateTimeToBeReady[] = "Translate.TimeToBeReady";
const char kTranslateTimeToLoad[] = "Translate.TimeToLoad";
const char kTranslateTimeToTranslate[] = "Translate.TimeToTranslate";

struct MetricsEntry {
  TranslateHelperMetrics::MetricsNameIndex index;
  const char* const name;
};

// This entry table should be updated when new UMA item is added.
const MetricsEntry kMetricsEntries[] = {
  { TranslateHelperMetrics::UMA_LANGUAGE_DETECTION,
    kRenderer4LanguageDetection },
  { TranslateHelperMetrics::UMA_CONTENT_LANGUAGE,
    kTranslateContentLanguage },
  { TranslateHelperMetrics::UMA_LANGUAGE_VERIFICATION,
    kTranslateLanguageVerification },
  { TranslateHelperMetrics::UMA_TIME_TO_BE_READY,
    kTranslateTimeToBeReady },
  { TranslateHelperMetrics::UMA_TIME_TO_LOAD,
    kTranslateTimeToLoad },
  { TranslateHelperMetrics::UMA_TIME_TO_TRANSLATE,
    kTranslateTimeToTranslate },
};

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

void ReportTimeToBeReady(double time_in_msec) {
  UMA_HISTOGRAM_MEDIUM_TIMES(
      kTranslateTimeToBeReady,
      base::TimeDelta::FromMicroseconds(time_in_msec * 1000.0));
}

void ReportTimeToLoad(double time_in_msec) {
  UMA_HISTOGRAM_MEDIUM_TIMES(
      kTranslateTimeToLoad,
      base::TimeDelta::FromMicroseconds(time_in_msec * 1000.0));
}

void ReportTimeToTranslate(double time_in_msec) {
  UMA_HISTOGRAM_MEDIUM_TIMES(
      kTranslateTimeToTranslate,
      base::TimeDelta::FromMicroseconds(time_in_msec * 1000.0));
}

#if defined(ENABLE_LANGUAGE_DETECTION)

void ReportLanguageDetectionTime(base::TimeTicks begin, base::TimeTicks end) {
  UMA_HISTOGRAM_MEDIUM_TIMES(kRenderer4LanguageDetection, end - begin);
}

#endif  // defined(ENABLE_LANGUAGE_DETECTION)

const char* GetMetricsName(MetricsNameIndex index) {
  for (size_t i = 0; i < arraysize(kMetricsEntries); ++i) {
    if (kMetricsEntries[i].index == index)
      return kMetricsEntries[i].name;
  }
  NOTREACHED();
  return NULL;
}

} // namespace TranslateHelperMetrics
