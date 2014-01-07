// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_util.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
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

  std::vector<std::string> chunks;
  base::SplitString(language, '-', &chunks);
  if (chunks.size() == 0u)
    return;

  *main_part = chunks[0];
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
// chrome/browser/resources/options/language_options.js
const LanguageCodePair kLanguageCodeSimilitudes[] = {
  {"no", "nb"},
  {"tl", "fil"},
};

// Some languages have changed codes over the years and sometimes the older
// codes are used, so we must see them as synonyms.
//
// If this table is updated, please sync this with the synonym table in
// chrome/browser/resources/options/language_options.js
const LanguageCodePair kLanguageCodeSynonyms[] = {
  {"iw", "he"},
  {"jw", "jv"},
};

const char kSecurityOrigin[] = "https://translate.googleapis.com/";

void ToTranslateLanguageSynonym(std::string* language) {
  for (size_t i = 0; i < arraysize(kLanguageCodeSimilitudes); ++i) {
    if (*language == kLanguageCodeSimilitudes[i].chrome_language) {
      *language = kLanguageCodeSimilitudes[i].translate_language;
      return;
    }
  }

  std::string main_part, tail_part;
  SplitIntoMainAndTail(*language, &main_part, &tail_part);
  if (main_part.empty())
    return;

  // Apply linear search here because number of items in the list is just four.
  for (size_t i = 0; i < arraysize(kLanguageCodeSynonyms); ++i) {
    if (main_part == kLanguageCodeSynonyms[i].chrome_language) {
      main_part = std::string(kLanguageCodeSynonyms[i].translate_language);
      break;
    }
  }

  *language = main_part + tail_part;
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
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTranslateSecurityOrigin)) {
    security_origin =
        command_line->GetSwitchValueASCII(switches::kTranslateSecurityOrigin);
  }
  return GURL(security_origin);
}

}  // namespace translate
