// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_ADDRESS_PARSER_INTERNAL_H_
#define ANDROID_WEBVIEW_BROWSER_ADDRESS_PARSER_INTERNAL_H_

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "base/strings/string_tokenizer.h"

namespace android_webview {

namespace address_parser {

// Internal classes and functions for address parsing.
namespace internal {

// Exposed for tests.
struct Word {
  base::string16::const_iterator begin;
  base::string16::const_iterator end;

  Word();
  Word(const base::string16::const_iterator& begin,
       const base::string16::const_iterator& end);
  Word(const Word& other);
};

// Exposed for tests.
class HouseNumberParser {
 public:
  HouseNumberParser();

  bool Parse(const base::string16::const_iterator& begin,
             const base::string16::const_iterator& end,
             Word* word);

 private:
  static inline bool IsPreDelimiter(base::char16 character);
  static inline bool IsPostDelimiter(base::char16 character);
  inline void RestartOnNextDelimiter();

  inline bool CheckFinished(Word* word) const;
  inline void AcceptChars(size_t num_chars);
  inline void SkipChars(size_t num_chars);
  inline void ResetState();

  // Iterators to the beginning, current position and ending of the string
  // being currently parsed.
  base::string16::const_iterator begin_;
  base::string16::const_iterator it_;
  base::string16::const_iterator end_;

  // Number of digits found in the current result candidate.
  size_t num_digits_;

  // Number of characters previous to the current iterator that belong
  // to the current result candidate.
  size_t result_chars_;

  DISALLOW_COPY_AND_ASSIGN(HouseNumberParser);
};

typedef std::vector<Word> WordList;
typedef base::StringTokenizerT<base::string16, base::string16::const_iterator>
    String16Tokenizer;

// These are exposed for tests.
bool FindStateStartingInWord(WordList* words,
                             size_t state_first_word,
                             size_t* state_last_word,
                             String16Tokenizer* tokenizer,
                             size_t* state_index);

bool IsValidLocationName(const Word& word);
bool IsZipValid(const Word& word, size_t state_index);
bool IsZipValidForState(const Word& word, size_t state_index);

}  // namespace internal

}  // namespace address_parser

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_ADDRESS_PARSER_INTERNAL_H_
