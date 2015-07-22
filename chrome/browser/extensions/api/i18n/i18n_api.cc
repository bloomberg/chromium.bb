// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/i18n/i18n_api.h"

#include <algorithm>
#include <vector>

#include "base/prefs/pref_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "third_party/cld_2/src/internal/compact_lang_det_impl.h"

namespace extensions {

namespace GetAcceptLanguages = api::i18n::GetAcceptLanguages;
using DetectedLanguage =
    api::i18n::DetectLanguage::Results::Result::LanguagesType;
using LanguageDetectionResult = api::i18n::DetectLanguage::Results::Result;

namespace {

// Max number of languages detected by CLD2.
const int kCldNumLangs = 3;

// Errors.
static const char kEmptyAcceptLanguagesError[] = "accept-languages is empty.";

}

bool I18nGetAcceptLanguagesFunction::RunSync() {
  std::string accept_languages =
      GetProfile()->GetPrefs()->GetString(prefs::kAcceptLanguages);
  // Currently, there are 2 ways to set browser's accept-languages: through UI
  // or directly modify the preference file. The accept-languages set through
  // UI is guranteed to be valid, and the accept-languages string returned from
  // profile()->GetPrefs()->GetString(prefs::kAcceptLanguages) is guranteed to
  // be valid and well-formed, which means each accept-langauge is a valid
  // code, and accept-languages are seperatd by "," without surrrounding
  // spaces. But we do not do any validation (either the format or the validity
  // of the language code) on accept-languages set through editing preference
  // file directly. So, here, we're adding extra checks to be resistant to
  // crashes caused by data corruption.
  if (accept_languages.empty()) {
    error_ = kEmptyAcceptLanguagesError;
    return false;
  }

  std::vector<std::string> languages = base::SplitString(
      accept_languages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  languages.erase(std::remove(languages.begin(), languages.end(), ""),
                  languages.end());

  if (languages.empty()) {
    error_ = kEmptyAcceptLanguagesError;
    return false;
  }

  results_ = GetAcceptLanguages::Results::Create(languages);
  return true;
}

ExtensionFunction::ResponseAction I18nDetectLanguageFunction::Run() {
  scoped_ptr<api::i18n::DetectLanguage::Params> params(
      api::i18n::DetectLanguage::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  return RespondNow(ArgumentList(GetLanguage(params->text)));
}

scoped_ptr<base::ListValue> I18nDetectLanguageFunction::GetLanguage(
    const std::string& text) {
  // TODO(mcindy): improve this by providing better CLD hints
  // asummed no cld hints is provided
  CLD2::CLDHints cldhints = {
      nullptr, "", CLD2::UNKNOWN_ENCODING, CLD2::UNKNOWN_LANGUAGE};

  bool is_plain_text = true;  // assume the text is a plain text
  int flags = 0;              // no flags, see compact_lang_det.h for details
  int text_bytes;             // amount of non-tag/letters-only text (assumed 0)
  int valid_prefix_bytes;     // amount of valid UTF8 character in the string
  double normalized_score[kCldNumLangs];

  CLD2::Language languages[kCldNumLangs];
  int percents[kCldNumLangs];
  bool is_reliable = false;

  // populating languages and percents
  int cld_language = CLD2::ExtDetectLanguageSummaryCheckUTF8(
      text.c_str(), static_cast<int>(text.size()), is_plain_text, &cldhints,
      flags, languages, percents, normalized_score,
      nullptr,  // assumed no ResultChunkVector is used
      &text_bytes, &is_reliable, &valid_prefix_bytes);

  // Check if non-UTF8 character is encountered
  // See bug http://crbug.com/444258.
  if (valid_prefix_bytes < static_cast<int>(text.size()) &&
      cld_language == CLD2::UNKNOWN_LANGUAGE) {
    // Detect Language upto before the first non-UTF8 character
    CLD2::DetectLanguageSummaryV2(
        text.c_str(), valid_prefix_bytes, is_plain_text, &cldhints,
        true,  // allow extended languages
        flags, CLD2::UNKNOWN_LANGUAGE, languages, percents, normalized_score,
        nullptr,  // assumed no ResultChunkVector is used
        &text_bytes, &is_reliable);
  }

  LanguageDetectionResult result;
  result.is_reliable = is_reliable;
  InitDetectedLanguages(languages, percents, &result.languages);
  return api::i18n::DetectLanguage::Results::Create(result);
}

void I18nDetectLanguageFunction::InitDetectedLanguages(
    CLD2::Language* languages,
    int* percents,
    std::vector<linked_ptr<DetectedLanguage>>* detected_languages) {
  for (int i = 0; i < kCldNumLangs; i++) {
    std::string language_code = "";

    // Convert LanguageCode 'zh' to 'zh-CN' and 'zh-Hant' to 'zh-TW' for
    // Translate server usage. see DetermineTextLanguage in
    // components/translate/core/language_detection/language_detection_util.cc
    if (languages[i] == CLD2::UNKNOWN_LANGUAGE) {
      // no need to save in detected_languages
      break;
    } else if (languages[i] == CLD2::CHINESE) {
      language_code = "zh-CN";
    } else if (languages[i] == CLD2::CHINESE_T) {
      language_code = "zh-TW";
    } else {
      language_code =
          CLD2::LanguageCode(static_cast<CLD2::Language>(languages[i]));
    }
    linked_ptr<DetectedLanguage> detected_lang(new DetectedLanguage);
    detected_lang->language = language_code;
    detected_lang->percentage = percents[i];
    detected_languages->push_back(detected_lang);
  }
}

}  // namespace extensions
