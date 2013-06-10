// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/translate/translate_util.h"

#include "base/basictypes.h"

namespace TranslateUtil {

// Language code synonyms. Some languages have changed codes over the years
// and sometimes the older codes are used, so we must see them as synonyms.
struct LanguageCodeSynonym {
  // Code used in supporting list.
  const char* const to;

  // Synonym code.
  const char* const from;
};

const LanguageCodeSynonym kLanguageCodeSynonyms[] = {
  {"no", "nb"},
  {"iw", "he"},
  {"jw", "jv"},
  {"tl", "fil"},
};

void ConvertLanguageCodeSynonym(std::string* language) {
  // Apply liner search here because number of items in the list is just four.
  for (size_t i = 0; i < arraysize(kLanguageCodeSynonyms); ++i) {
    if (language->compare(kLanguageCodeSynonyms[i].from) == 0) {
      *language = std::string(kLanguageCodeSynonyms[i].to);
      break;
    }
  }
}

}  // namespace TranslateUtil
