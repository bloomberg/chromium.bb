// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/spellchecker/spellcheck_language.h"

#include "base/logging.h"
#include "chrome/renderer/spellchecker/spellcheck_worditerator.h"
#include "chrome/renderer/spellchecker/spelling_engine.h"


SpellcheckLanguage::SpellcheckLanguage()
    : platform_spelling_engine_(CreateNativeSpellingEngine()) {
}

SpellcheckLanguage::~SpellcheckLanguage() {
}

void SpellcheckLanguage::Init(base::File file, const std::string& language) {
  DCHECK(platform_spelling_engine_.get());
  platform_spelling_engine_->Init(file.Pass());

  character_attributes_.SetDefaultLanguage(language);
  text_iterator_.Reset();
  contraction_iterator_.Reset();
}

bool SpellcheckLanguage::InitializeIfNeeded() {
  DCHECK(platform_spelling_engine_.get());
  return platform_spelling_engine_->InitializeIfNeeded();
}

bool SpellcheckLanguage::SpellCheckWord(
    const base::char16* in_word,
    int in_word_len,
    int tag,
    int* misspelling_start,
    int* misspelling_len,
    std::vector<base::string16>* optional_suggestions) {
  DCHECK(in_word_len >= 0);
  DCHECK(misspelling_start && misspelling_len) << "Out vars must be given.";

  // Do nothing if we need to delay initialization. (Rather than blocking,
  // report the word as correctly spelled.)
  if (InitializeIfNeeded())
    return true;

  // Do nothing if spell checking is disabled.
  if (!platform_spelling_engine_.get() ||
      !platform_spelling_engine_->IsEnabled())
    return true;

  *misspelling_start = 0;
  *misspelling_len = 0;
  if (in_word_len == 0)
    return true;  // No input means always spelled correctly.

  base::string16 word;
  int word_start;
  int word_length;
  if (!text_iterator_.IsInitialized() &&
      !text_iterator_.Initialize(&character_attributes_, true)) {
      // We failed to initialize text_iterator_, return as spelled correctly.
      VLOG(1) << "Failed to initialize SpellcheckWordIterator";
      return true;
  }

  text_iterator_.SetText(in_word, in_word_len);
  DCHECK(platform_spelling_engine_.get());
  while (text_iterator_.GetNextWord(&word, &word_start, &word_length)) {
    // Found a word (or a contraction) that the spellchecker can check the
    // spelling of.
    if (platform_spelling_engine_->CheckSpelling(word, tag))
      continue;

    // If the given word is a concatenated word of two or more valid words
    // (e.g. "hello:hello"), we should treat it as a valid word.
    if (IsValidContraction(word, tag))
      continue;

    *misspelling_start = word_start;
    *misspelling_len = word_length;

    // Get the list of suggested words.
    if (optional_suggestions) {
      platform_spelling_engine_->FillSuggestionList(word,
                                                    optional_suggestions);
    }
    return false;
  }

  return true;
}

// Returns whether or not the given string is a valid contraction.
// This function is a fall-back when the SpellcheckWordIterator class
// returns a concatenated word which is not in the selected dictionary
// (e.g. "in'n'out") but each word is valid.
bool SpellcheckLanguage::IsValidContraction(const base::string16& contraction,
                                            int tag) {
  if (!contraction_iterator_.IsInitialized() &&
      !contraction_iterator_.Initialize(&character_attributes_, false)) {
    // We failed to initialize the word iterator, return as spelled correctly.
    VLOG(1) << "Failed to initialize contraction_iterator_";
    return true;
  }

  contraction_iterator_.SetText(contraction.c_str(), contraction.length());

  base::string16 word;
  int word_start;
  int word_length;

  DCHECK(platform_spelling_engine_.get());
  while (contraction_iterator_.GetNextWord(&word, &word_start, &word_length)) {
    if (!platform_spelling_engine_->CheckSpelling(word, tag))
      return false;
  }
  return true;
}

bool SpellcheckLanguage::IsEnabled() {
  DCHECK(platform_spelling_engine_.get());
  return platform_spelling_engine_->IsEnabled();
}
