// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SPELLCHECKER_HUNSPELL_ENGINE_H_
#define CHROME_RENDERER_SPELLCHECKER_HUNSPELL_ENGINE_H_

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/renderer/spellchecker/spelling_engine.h"

#include <string>
#include <vector>

class Hunspell;

class HunspellEngine : public SpellingEngine {
 public:
  HunspellEngine();
  virtual ~HunspellEngine();

  virtual void Init(base::PlatformFile file,
                    const std::vector<std::string>& custom_words) OVERRIDE;

  virtual bool InitializeIfNeeded() OVERRIDE;
  virtual bool IsEnabled() OVERRIDE;
  virtual bool CheckSpelling(const string16& word_to_check, int tag) OVERRIDE;
  virtual void FillSuggestionList(const string16& wrong_word,
                          std::vector<string16>* optional_suggestions) OVERRIDE;
  virtual void OnCustomDictionaryChanged(
      const std::vector<std::string>& words_added,
      const std::vector<std::string>& words_removed) OVERRIDE;

 private:
  // Initializes the Hunspell dictionary, or does nothing if |hunspell_| is
  // non-null. This blocks.
  void InitializeHunspell();

  // Add the given custom words to |hunspell_|.
  void AddWordsToHunspell(const std::vector<std::string>& words);

  // Remove the given custom words from |hunspell_|.
  void RemoveWordsFromHunspell(const std::vector<std::string>& words);

  // We memory-map the BDict file.
  scoped_ptr<file_util::MemoryMappedFile> bdict_file_;

  // The hunspell dictionary in use.
  scoped_ptr<Hunspell> hunspell_;

  chrome::spellcheck_common::WordList custom_words_;

  base::PlatformFile file_;

  // This flags is true if we have been initialized.
  // The value indicates whether we should request a
  // dictionary from the browser when the render view asks us to check the
  // spelling of a word.
  bool initialized_;

  // This flags is true if we have requested dictionary.
  bool dictionary_requested_;

};

#endif  // CHROME_RENDERER_SPELLCHECKER_HUNSPELL_ENGINE_H_

