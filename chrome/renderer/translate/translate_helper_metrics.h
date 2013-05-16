// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_TRANSLATE_TRANSLATE_HELPER_METRICS_H_
#define CHROME_RENDERER_TRANSLATE_TRANSLATE_HELPER_METRICS_H_

#include <string>

#include "base/time.h"

namespace TranslateHelperMetrics {

// An indexing type to query each UMA entry name via GetMetricsName() function.
// Note: |kMetricsEntries| should be updated when a new entry is added here.
enum MetricsNameIndex {
  UMA_LANGUAGE_DETECTION,
  UMA_CONTENT_LANGUAGE,
  UMA_LANGUAGE_VERIFICATION,
  UMA_TIME_TO_BE_READY,
  UMA_TIME_TO_LOAD,
  UMA_TIME_TO_TRANSLATE,
  UMA_MAX,
};

// A page may provide a Content-Language HTTP header or a META tag.
// TranslateHelper checks if a server provides a valid Content-Language.
enum ContentLanguageType {
  CONTENT_LANGUAGE_NOT_PROVIDED,
  CONTENT_LANGUAGE_VALID,
  CONTENT_LANGUAGE_INVALID,
  CONTENT_LANGUAGE_MAX,
};

// When a valid Content-Language is provided, TranslateHelper checks if a
// server provided Content-Language matches to a language CLD determined.
enum LanguageVerificationType {
  LANGUAGE_VERIFICATION_CLD_DISABLED,
  LANGUAGE_VERIFICATION_CLD_ONLY,
  LANGUAGE_VERIFICATION_UNKNOWN,
  LANGUAGE_VERIFICATION_CLD_AGREE,
  LANGUAGE_VERIFICATION_CLD_DISAGREE,
  LANGUAGE_VERIFICATION_MAX,
};

// Called after TranslateHelper verifies a server providing Content-Language
// header. |provided_code| contains a Content-Language header value which
// server provides. It can be empty string when a server doesn't provide it.
// |revised_code| is a value modified by format error corrector.
void ReportContentLanguage(const std::string& provided_code,
                           const std::string& revised_code);

// Called when CLD verifies Content-Language header.
void ReportLanguageVerification(LanguageVerificationType type);

// Called when the Translate Element library is ready.
void ReportTimeToBeReady(double time_in_msec);

// Called when the Translate Element library is loaded.
void ReportTimeToLoad(double time_in_msec);

// Called when a page translation is finished.
void ReportTimeToTranslate(double time_in_msec);

#if defined(ENABLE_LANGUAGE_DETECTION)

// Called when CLD detects page language.
void ReportLanguageDetectionTime(base::TimeTicks begin, base::TimeTicks end);

#endif  // defined(ENABLE_LANGUAGE_DETECTION)

// Gets UMA name for an entry specified by |index|.
const char* GetMetricsName(MetricsNameIndex index);

}  // namespace TranslateHelperMetrics

#endif  // CHROME_RENDERER_TRANSLATE_TRANSLATE_HELPER_METRICS_H_
