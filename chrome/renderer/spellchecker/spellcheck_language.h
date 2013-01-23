// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_LANGUAGE_H_
#define CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_LANGUAGE_H_

#include <queue>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/string16.h"
#include "chrome/renderer/spellchecker/spellcheck_worditerator.h"

class SpellingEngine;

class SpellcheckLanguage {
 public:
  SpellcheckLanguage();
  ~SpellcheckLanguage();

  void Init(base::PlatformFile file,
            const std::vector<std::string>& custom_words,
            const std::string& language);

  // Update custom dictionary.
  void OnCustomDictionaryChanged(
    const std::vector<std::string>& words_added,
    const std::vector<std::string>& words_removed);

  // SpellCheck a word.
  // Returns true if spelled correctly, false otherwise.
  // If the spellchecker failed to initialize, always returns true.
  // TODO(groby): This is not true in the multilingual case any more!
  // The |tag| parameter should either be a unique identifier for the document
  // that the word came from (if the current platform requires it), or 0.
  // In addition, finds the suggested words for a given word
  // and puts them into |*optional_suggestions|.
  // If the word is spelled correctly, the vector is empty.
  // If optional_suggestions is NULL, suggested words will not be looked up.
  // Note that doing suggest lookups can be slow.
  bool SpellCheckWord(const char16* in_word,
                      int in_word_len,
                      int tag,
                      int* misspelling_start,
                      int* misspelling_len,
                      std::vector<string16>* optional_suggestions);

  // Initialize |spellcheck_| if that hasn't happened yet.
  bool InitializeIfNeeded();

  // Return true if the underlying spellcheck engine is enabled.
  bool IsEnabled();

 private:
  friend class SpellCheckTest;

  // When called, relays the request to check the spelling to the proper
  // backend, either hunspell or a platform-specific backend.
  bool CheckSpelling(const string16& word_to_check, int tag);

  // When called, relays the request to fill the list with suggestions to
  // the proper backend, either hunspell or a platform-specific backend.
  void FillSuggestionList(const string16& wrong_word,
                          std::vector<string16>* optional_suggestions);

  // Returns whether or not the given word is a contraction of valid words
  // (e.g. "word:word").
  bool IsValidContraction(const string16& word, int tag);

  // Represents character attributes used for filtering out characters which
  // are not supported by this SpellCheck object.
  SpellcheckCharAttribute character_attributes_;

  // Represents word iterators used in this spellchecker. The |text_iterator_|
  // splits text provided by WebKit into words, contractions, or concatenated
  // words. The |contraction_iterator_| splits a concatenated word extracted by
  // |text_iterator_| into word components so we can treat a concatenated word
  // consisting only of correct words as a correct word.
  SpellcheckWordIterator text_iterator_;
  SpellcheckWordIterator contraction_iterator_;

  // Pointer to a platform-specific spelling engine, if it is in use. This
  // should only be set if hunspell is not used. (I.e. on OSX, for now)
  scoped_ptr<SpellingEngine> platform_spelling_engine_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckLanguage);
};

#endif  // CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_LANGUAGE_H_
