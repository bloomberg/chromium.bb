// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_METRICS_H_
#define COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_METRICS_H_

#include <string>

#include "base/time/time.h"

namespace translate {

// An indexing type to query each UMA entry name via GetMetricsName() function.
// Note: |kMetricsEntries| should be updated when a new entry is added here.
enum MetricsNameIndex {
  UMA_LANGUAGE_DETECTION,
  UMA_CONTENT_LANGUAGE,
  UMA_HTML_LANG,
  UMA_LANGUAGE_VERIFICATION,
  UMA_TIME_TO_BE_READY,
  UMA_TIME_TO_LOAD,
  UMA_TIME_TO_TRANSLATE,
  UMA_USER_ACTION_DURATION,
  UMA_PAGE_SCHEME,
  UMA_SIMILAR_LANGUAGE_MATCH,
  UMA_MAX,
};

// A page may provide a Content-Language HTTP header or a META tag.
// TranslateHelper checks if a server provides a valid Content-Language.
enum LanguageCheckType {
  LANGUAGE_NOT_PROVIDED,
  LANGUAGE_VALID,
  LANGUAGE_INVALID,
  LANGUAGE_MAX,
};

// When a valid Content-Language is provided, TranslateHelper checks if a
// server provided Content-Language matches to a language CLD determined.
enum LanguageVerificationType {
  LANGUAGE_VERIFICATION_CLD_DISABLED,  // obsolete
  LANGUAGE_VERIFICATION_CLD_ONLY,
  LANGUAGE_VERIFICATION_UNKNOWN,
  LANGUAGE_VERIFICATION_CLD_AGREE,
  LANGUAGE_VERIFICATION_CLD_DISAGREE,
  LANGUAGE_VERIFICATION_TRUST_CLD,
  LANGUAGE_VERIFICATION_CLD_COMPLEMENT_SUB_CODE,
  LANGUAGE_VERIFICATION_MAX,
};

// Scheme type of pages Chrome is going to translate.
enum SchemeType {
  SCHEME_HTTP,
  SCHEME_HTTPS,
  SCHEME_OTHERS,
  SCHEME_MAX,
};

// Called after TranslateHelper verifies a server providing Content-Language
// header. |provided_code| contains a Content-Language header value which a
// server provides. It can be empty string when a server doesn't provide it.
// |revised_code| is a value modified by format error corrector.
void ReportContentLanguage(const std::string& provided_code,
                           const std::string& revised_code);

// Called after TranslateHelper verifies a page providing html lang attribute.
// |provided_code| contains a html lang attribute which a page provides. It can
// be empty string when a page doesn't provide it. |revised_code| is a value
// modified by format error corrector.
void ReportHtmlLang(const std::string& provided_code,
                    const std::string& revised_code);

// Called when CLD verifies Content-Language header.
void ReportLanguageVerification(LanguageVerificationType type);

// Called when the Translate Element library is ready.
void ReportTimeToBeReady(double time_in_msec);

// Called when the Translate Element library is loaded.
void ReportTimeToLoad(double time_in_msec);

// Called when a page translation is finished.
void ReportTimeToTranslate(double time_in_msec);

// Called when a translation is triggered.
void ReportUserActionDuration(base::TimeTicks begin, base::TimeTicks end);

// Called when a translation is triggered.
void ReportPageScheme(const std::string& scheme);

// Called when CLD detects page language.
void ReportLanguageDetectionTime(base::TimeTicks begin, base::TimeTicks end);

// Called when CLD agreed on a language which is different, but in the similar
// language list.
void ReportSimilarLanguageMatch(bool match);

// Gets UMA name for an entry specified by |index|.
const char* GetMetricsName(MetricsNameIndex index);

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_METRICS_H_
