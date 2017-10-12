// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_util.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "components/translate/core/common/translate_switches.h"
#include "url/gurl.h"

namespace {

// Split the |language| into two parts. For example, if |language| is 'en-US',
// this will be split into the main part 'en' and the tail part '-US'.
void SplitIntoMainAndTail(const std::string& language,
                          std::string* main_part,
                          std::string* tail_part) {
  DCHECK(main_part);
  DCHECK(tail_part);

  std::vector<base::StringPiece> chunks = base::SplitStringPiece(
      language, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (chunks.empty())
    return;

  chunks[0].CopyToString(main_part);
  *tail_part = language.substr(main_part->size());
}

}  // namespace

namespace translate {

struct LanguageCodePair {
  // Code used in supporting list of Translate.
  const char* const translate_language;

  // Code used in Chrome internal.
  const char* const chrome_language;
};

// Some languages are treated as same languages in Translate even though they
// are different to be exact.
//
// If this table is updated, please sync this with the synonym table in
// chrome/browser/resources/settings/languages_page/languages.js.
const LanguageCodePair kLanguageCodeSimilitudes[] = {
  {"no", "nb"},
  {"tl", "fil"},
};

// Some languages have changed codes over the years and sometimes the older
// codes are used, so we must see them as synonyms.
//
// If this table is updated, please sync this with the synonym table in
// chrome/browser/resources/settings/languages_page/languages.js.
const LanguageCodePair kLanguageCodeSynonyms[] = {
  {"iw", "he"},
  {"jw", "jv"},
};

// Some Chinese language codes are compatible with zh-TW or zh-CN in terms of
// Translate.
//
// If this table is updated, please sync this with the synonym table in
// chrome/browser/resources/settings/languages_page/languages.js.
const LanguageCodePair kLanguageCodeChineseCompatiblePairs[] = {
    {"zh-TW", "zh-HK"},
    {"zh-TW", "zh-MO"},
    {"zh-CN", "zh-SG"},
    {"zh-CN", "zh"},
};

const char kSecurityOrigin[] = "https://translate.googleapis.com/";

void ToTranslateLanguageSynonym(std::string* language) {
  for (size_t i = 0; i < arraysize(kLanguageCodeSimilitudes); ++i) {
    if (*language == kLanguageCodeSimilitudes[i].chrome_language) {
      *language = kLanguageCodeSimilitudes[i].translate_language;
      return;
    }
  }

  for (size_t i = 0; i < arraysize(kLanguageCodeChineseCompatiblePairs); ++i) {
    if (*language == kLanguageCodeChineseCompatiblePairs[i].chrome_language) {
      *language = kLanguageCodeChineseCompatiblePairs[i].translate_language;
      return;
    }
  }

  std::string main_part, tail_part;
  SplitIntoMainAndTail(*language, &main_part, &tail_part);
  if (main_part.empty())
    return;

  // Chinese is a special case.
  // There is not a single base language, but two: traditional and simplified.
  // The kLanguageCodeChineseCompatiblePairs list contains the relation between
  // various Chinese locales.
  if (main_part == "zh") {
    *language = main_part + tail_part;
    return;
  }

  // Apply linear search here because number of items in the list is just four.
  for (size_t i = 0; i < arraysize(kLanguageCodeSynonyms); ++i) {
    if (main_part == kLanguageCodeSynonyms[i].chrome_language) {
      main_part = std::string(kLanguageCodeSynonyms[i].translate_language);
      break;
    }
  }

  *language = main_part;
}

void ToChromeLanguageSynonym(std::string* language) {
  for (size_t i = 0; i < arraysize(kLanguageCodeSimilitudes); ++i) {
    if (*language == kLanguageCodeSimilitudes[i].translate_language) {
      *language = kLanguageCodeSimilitudes[i].chrome_language;
      return;
    }
  }

  std::string main_part, tail_part;
  SplitIntoMainAndTail(*language, &main_part, &tail_part);
  if (main_part.empty())
    return;

  // Apply liner search here because number of items in the list is just four.
  for (size_t i = 0; i < arraysize(kLanguageCodeSynonyms); ++i) {
    if (main_part == kLanguageCodeSynonyms[i].translate_language) {
      main_part = std::string(kLanguageCodeSynonyms[i].chrome_language);
      break;
    }
  }

  *language = main_part + tail_part;
}

GURL GetTranslateSecurityOrigin() {
  std::string security_origin(kSecurityOrigin);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTranslateSecurityOrigin)) {
    security_origin =
        command_line->GetSwitchValueASCII(switches::kTranslateSecurityOrigin);
  }
  return GURL(security_origin);
}

bool ContainsSameBaseLanguage(const std::vector<std::string>& list,
                              const std::string& language_code) {
  std::string base_language;
  std::string unused_str;
  SplitIntoMainAndTail(language_code, &base_language, &unused_str);
  for (const auto& item : list) {
    std::string compare_base;
    SplitIntoMainAndTail(item, &compare_base, &unused_str);
    if (compare_base == base_language)
      return true;
  }

  return false;
}

}  // namespace translate
