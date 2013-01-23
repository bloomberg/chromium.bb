// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SPELLCHECKER_NSSPELLCHECKER_ENGINE_H_
#define CHROME_RENDERER_SPELLCHECKER_NSSPELLCHECKER_ENGINE_H_

#include "base/compiler_specific.h"
#include "chrome/renderer/spellchecker/spelling_engine.h"

class CocoaSpellingEngine : public SpellingEngine {
 public:
  virtual void Init(base::PlatformFile bdict_file,
                    const std::vector<std::string>& custom_words) OVERRIDE;
  virtual bool InitializeIfNeeded() OVERRIDE;
  virtual bool IsEnabled() OVERRIDE;
  virtual bool CheckSpelling(const string16& word_to_check, int tag) OVERRIDE;
  virtual void FillSuggestionList(const string16& wrong_word,
                          std::vector<string16>* optional_suggestions) OVERRIDE;
  virtual void OnCustomDictionaryChanged(
      const std::vector<std::string>& words_added,
      const std::vector<std::string>& words_removed) OVERRIDE;
};

#endif  // CHROME_RENDERER_SPELLCHECKER_NSSPELLCHECKER_ENGINE_H_

