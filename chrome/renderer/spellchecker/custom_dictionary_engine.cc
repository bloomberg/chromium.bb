// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/spellchecker/custom_dictionary_engine.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"

CustomDictionaryEngine::CustomDictionaryEngine() {
}

CustomDictionaryEngine::~CustomDictionaryEngine() {
}

void CustomDictionaryEngine::Init(
    const std::vector<std::string>& custom_words) {
  // SpellingMenuOberver calls UTF16ToUTF8(word) to convert words for storage,
  // synchronization, and use in the custom dictionary engine. Since
  // (UTF8ToUTF16(UTF16ToUTF8(word)) == word) holds, the engine does not need to
  // normalize the strings.
  for (std::vector<std::string>::const_iterator it = custom_words.begin();
       it != custom_words.end();
       ++it) {
    dictionary_.insert(UTF8ToUTF16(*it));
  }
}

void CustomDictionaryEngine::OnCustomDictionaryChanged(
    const std::vector<std::string>& words_added,
    const std::vector<std::string>& words_removed) {
  for (std::vector<std::string>::const_iterator it = words_added.begin();
       it != words_added.end();
       ++it) {
    dictionary_.insert(UTF8ToUTF16(*it));
  }
  for (std::vector<std::string>::const_iterator it = words_removed.begin();
       it != words_removed.end();
       ++it) {
    dictionary_.erase(UTF8ToUTF16(*it));
  }
}

bool CustomDictionaryEngine::SpellCheckWord(
    const char16* text,
    int misspelling_start,
    int misspelling_len) {
  DCHECK(text);
  return misspelling_start >= 0 &&
      misspelling_len > 0 &&
      dictionary_.count(string16(text, misspelling_start, misspelling_len)) > 0;
}
