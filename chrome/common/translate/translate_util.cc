// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/translate/translate_util.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "url/gurl.h"

namespace TranslateUtil {

// Language code synonyms. Some languages have changed codes over the years
// and sometimes the older codes are used, so we must see them as synonyms.
struct LanguageCodeSynonym {
  // Code used in supporting list of Translate.
  const char* const translate_language;

  // Code used in Chrome internal.
  const char* const chrome_language;
};

// If this table is updated, please sync this with that in
// chrome/browser/resources/options/language_options.js
const LanguageCodeSynonym kLanguageCodeSynonyms[] = {
  {"no", "nb"},
  {"iw", "he"},
  {"jw", "jv"},
  {"tl", "fil"},
};

const char kSecurityOrigin[] = "https://translate.googleapis.com/";

void ToTranslateLanguageSynonym(std::string* language) {
  // Apply liner search here because number of items in the list is just four.
  for (size_t i = 0; i < arraysize(kLanguageCodeSynonyms); ++i) {
    if (language->compare(kLanguageCodeSynonyms[i].chrome_language) == 0) {
      *language = std::string(kLanguageCodeSynonyms[i].translate_language);
      break;
    }
  }
}

void ToChromeLanguageSynonym(std::string* language) {
  // Apply liner search here because number of items in the list is just four.
  for (size_t i = 0; i < arraysize(kLanguageCodeSynonyms); ++i) {
    if (language->compare(kLanguageCodeSynonyms[i].translate_language) == 0) {
      *language = std::string(kLanguageCodeSynonyms[i].chrome_language);
      break;
    }
  }
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

}  // namespace TranslateUtil
