// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_manager_metrics.h"

#include "base/basictypes.h"
#include "base/metrics/histogram.h"

namespace {

// Constant string values to indicate UMA names. All entries should have
// a corresponding index in MetricsNameIndex and an entry in |kMetricsEntries|.
const char kTranslateInitiationStatus[] =
    "Translate.InitiationStatus";
const char kTranslateReportLanguageDetectionError[] =
    "Translate.ReportLanguageDetectionError";
const char kTranslateServerReportedUnsupportedLanguage[] =
    "Translate.ServerReportedUnsupportedLanguage";

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
  UMA_HISTOGRAM_COUNTS(kTranslateReportLanguageDetectionError, 1);
}

void ReportUnsupportedLanguage() {
  UMA_HISTOGRAM_COUNTS(kTranslateServerReportedUnsupportedLanguage, 1);
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
