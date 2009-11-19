// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/spellchecker/spellcheck.h"

#include "base/file_util.h"
#include "base/histogram.h"
#include "base/time.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/renderer/render_thread.h"
#include "third_party/hunspell/src/hunspell/hunspell.hxx"

using base::TimeTicks;

SpellCheck::SpellCheck()
    : file_(base::kInvalidPlatformFileValue),
      auto_spell_correct_turned_on_(false),
      is_using_platform_spelling_engine_(false),
      initialized_(false) {
  // Wait till we check the first word before doing any initializing.
}

SpellCheck::~SpellCheck() {
}

void SpellCheck::Init(base::PlatformFile file,
                      const std::vector<std::string>& custom_words,
                      const std::string language) {
  initialized_ = true;
  hunspell_.reset();
  bdict_file_.reset();
  file_ = file;
  is_using_platform_spelling_engine_ =
      file == base::kInvalidPlatformFileValue && !language.empty();

  character_attributes_.SetDefaultLanguage(language);

  custom_words_.insert(custom_words_.end(),
                       custom_words.begin(), custom_words.end());

  // We delay the actual initialization of hunspell until it is needed.
}

bool SpellCheck::SpellCheckWord(
    const char16* in_word,
    int in_word_len,
    int tag,
    int* misspelling_start,
    int* misspelling_len,
    std::vector<string16>* optional_suggestions) {
  DCHECK(in_word_len >= 0);
  DCHECK(misspelling_start && misspelling_len) << "Out vars must be given.";

  // Do nothing if we need to delay initialization. (Rather than blocking,
  // report the word as correctly spelled.)
  if (InitializeIfNeeded())
    return true;

  // Do nothing if spell checking is disabled.
  if (initialized_ && file_ == base::kInvalidPlatformFileValue &&
      !is_using_platform_spelling_engine_) {
    return true;
  }

  *misspelling_start = 0;
  *misspelling_len = 0;
  if (in_word_len == 0)
    return true;  // No input means always spelled correctly.

  SpellcheckWordIterator word_iterator;
  string16 word;
  int word_start;
  int word_length;
  word_iterator.Initialize(&character_attributes_, in_word, in_word_len, true);
  while (word_iterator.GetNextWord(&word, &word_start, &word_length)) {
    // Found a word (or a contraction) that the spellchecker can check the
    // spelling of.
    if (CheckSpelling(word, tag))
      continue;

    // If the given word is a concatenated word of two or more valid words
    // (e.g. "hello:hello"), we should treat it as a valid word.
    if (IsValidContraction(word, tag))
      continue;

    *misspelling_start = word_start;
    *misspelling_len = word_length;

    // Get the list of suggested words.
    if (optional_suggestions)
      FillSuggestionList(word, optional_suggestions);
    return false;
  }

  return true;
}

string16 SpellCheck::GetAutoCorrectionWord(const string16& word, int tag) {
  string16 autocorrect_word;
  if (!auto_spell_correct_turned_on_)
    return autocorrect_word;  // Return the empty string.

  int word_length = static_cast<int>(word.size());
  if (word_length < 2 || word_length > SpellCheckCommon::kMaxAutoCorrectWordSize)
    return autocorrect_word;

  if (InitializeIfNeeded())
    return autocorrect_word;

  char16 misspelled_word[SpellCheckCommon::kMaxAutoCorrectWordSize + 1];
  const char16* word_char = word.c_str();
  for (int i = 0; i <= SpellCheckCommon::kMaxAutoCorrectWordSize; i++) {
    if (i >= word_length)
      misspelled_word[i] = NULL;
    else
      misspelled_word[i] = word_char[i];
  }

  // Swap adjacent characters and spellcheck.
  int misspelling_start, misspelling_len;
  for (int i = 0; i < word_length - 1; i++) {
    // Swap.
    std::swap(misspelled_word[i], misspelled_word[i + 1]);

    // Check spelling.
    misspelling_start = misspelling_len = 0;
    SpellCheckWord(misspelled_word, word_length, tag, &misspelling_start,
        &misspelling_len, NULL);

    // Make decision: if only one swap produced a valid word, then we want to
    // return it. If we found two or more, we don't do autocorrection.
    if (misspelling_len == 0) {
      if (autocorrect_word.empty()) {
        autocorrect_word.assign(misspelled_word);
      } else {
        autocorrect_word.clear();
        break;
      }
    }

    // Restore the swapped characters.
    std::swap(misspelled_word[i], misspelled_word[i + 1]);
  }
  return autocorrect_word;
}

void SpellCheck::EnableAutoSpellCorrect(bool turn_on) {
  auto_spell_correct_turned_on_ = turn_on;
}

void SpellCheck::WordAdded(const std::string& word) {
  if (is_using_platform_spelling_engine_)
    return;

  if (!hunspell_.get()) {
    // Save it for later---add it when hunspell is initialized.
    custom_words_.push_back(word);
  } else {
    AddWordToHunspell(word);
  }
}

void SpellCheck::InitializeHunspell() {
  if (hunspell_.get())
    return;

  bdict_file_.reset(new file_util::MemoryMappedFile);

  if (bdict_file_->Initialize(file_)) {
    TimeTicks start_time = TimeTicks::Now();

    hunspell_.reset(
        new Hunspell(bdict_file_->data(), bdict_file_->length()));

    // Add custom words to Hunspell.
    for (std::vector<std::string>::iterator it = custom_words_.begin();
         it != custom_words_.end(); ++it) {
      AddWordToHunspell(*it);
    }

    DHISTOGRAM_TIMES("Spellcheck.InitTime",
                     TimeTicks::Now() - start_time);
  }
}

void SpellCheck::AddWordToHunspell(const std::string& word) {
  if (!word.empty() && word.length() < MAXWORDUTF8LEN)
    hunspell_->add(word.c_str());
}

bool SpellCheck::InitializeIfNeeded() {
  if (is_using_platform_spelling_engine_)
    return false;

  if (!initialized_) {
    RenderThread::current()->Send(
        new ViewHostMsg_SpellChecker_RequestDictionary);
    initialized_ = true;
    return true;
  }

  // Check if the platform spellchecker is being used.
  if (file_ != base::kInvalidPlatformFileValue) {
    // If it isn't, init hunspell.
    InitializeHunspell();
  }

  return false;
}

// When called, relays the request to check the spelling to the proper
// backend, either hunspell or a platform-specific backend.
bool SpellCheck::CheckSpelling(const string16& word_to_check, int tag) {
  bool word_correct = false;

  if (is_using_platform_spelling_engine_) {
    RenderThread::current()->Send(
        new ViewHostMsg_SpellChecker_PlatformCheckSpelling(word_to_check, tag,
                                                           &word_correct));
  } else {
    std::string word_to_check_utf8(UTF16ToUTF8(word_to_check));
    // Hunspell shouldn't let us exceed its max, but check just in case
    if (word_to_check_utf8.length() < MAXWORDUTF8LEN) {
      // |hunspell_->spell| returns 0 if the word is spelled correctly and
      // non-zero otherwsie.
      word_correct = (hunspell_->spell(word_to_check_utf8.c_str()) != 0);
    }
  }

  return word_correct;
}

void SpellCheck::FillSuggestionList(
    const string16& wrong_word,
    std::vector<string16>* optional_suggestions) {
  if (is_using_platform_spelling_engine_) {
    RenderThread::current()->Send(
        new ViewHostMsg_SpellChecker_PlatformFillSuggestionList(
            wrong_word, optional_suggestions));
    return;
  }
  char** suggestions;
  int number_of_suggestions =
      hunspell_->suggest(&suggestions, UTF16ToUTF8(wrong_word).c_str());

  // Populate the vector of WideStrings.
  for (int i = 0; i < number_of_suggestions; i++) {
    if (i < SpellCheckCommon::kMaxSuggestions)
      optional_suggestions->push_back(UTF8ToUTF16(suggestions[i]));
    free(suggestions[i]);
  }
  if (suggestions != NULL)
    free(suggestions);
}

// Returns whether or not the given string is a valid contraction.
// This function is a fall-back when the SpellcheckWordIterator class
// returns a concatenated word which is not in the selected dictionary
// (e.g. "in'n'out") but each word is valid.
bool SpellCheck::IsValidContraction(const string16& contraction, int tag) {
  SpellcheckWordIterator word_iterator;
  word_iterator.Initialize(&character_attributes_, contraction.c_str(),
                           contraction.length(), false);

  string16 word;
  int word_start;
  int word_length;
  while (word_iterator.GetNextWord(&word, &word_start, &word_length)) {
    if (!CheckSpelling(word, tag))
      return false;
  }
  return true;
}
