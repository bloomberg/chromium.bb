// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_ADDRESS_DETECTOR_H_
#define CONTENT_RENDERER_ANDROID_ADDRESS_DETECTOR_H_
#pragma once

#include <vector>

#include "base/string_tokenizer.h"
#include "content/renderer/android/content_detector.h"

class AddressDetectorTest;

namespace content {

// Finds a geographical address (currently US only) in the given text string.
class AddressDetector : public ContentDetector {
 public:
  AddressDetector();
  virtual ~AddressDetector();

 private:
  friend class ::AddressDetectorTest;

  // Implementation of ContentDetector.
  virtual bool FindContent(const string16::const_iterator& begin,
                           const string16::const_iterator& end,
                           size_t* start_pos,
                           size_t* end_pos,
                           std::string* content_text) OVERRIDE;
  virtual GURL GetIntentURL(const std::string& content_text) OVERRIDE;
  virtual size_t GetMaximumContentLength() OVERRIDE;

  std::string GetContentText(const string16& text);

  // Internal structs and classes. Required to be visible by the unit tests.
  struct Word {
    string16::const_iterator begin;
    string16::const_iterator end;

    Word() {}
    Word(const string16::const_iterator& begin_it,
         const string16::const_iterator& end_it)
        : begin(begin_it),
          end(end_it) {
      DCHECK(begin_it <= end_it);
    }
  };

  class HouseNumberParser {
   public:
    HouseNumberParser() {}

    bool Parse(const string16::const_iterator& begin,
               const string16::const_iterator& end,
               Word* word);

   private:
    static inline bool IsPreDelimiter(char16 character);
    static inline bool IsPostDelimiter(char16 character);
    inline void RestartOnNextDelimiter();

    inline bool CheckFinished(Word* word) const;
    inline void AcceptChars(size_t num_chars);
    inline void SkipChars(size_t num_chars);
    inline void ResetState();

    // Iterators to the beginning, current position and ending of the string
    // being currently parsed.
    string16::const_iterator begin_;
    string16::const_iterator it_;
    string16::const_iterator end_;

    // Number of digits found in the current result candidate.
    size_t num_digits_;

    // Number of characters previous to the current iterator that belong
    // to the current result candidate.
    size_t result_chars_;

    DISALLOW_COPY_AND_ASSIGN(HouseNumberParser);
  };

  typedef std::vector<Word> WordList;
  typedef StringTokenizerT<string16, string16::const_iterator>
      String16Tokenizer;

  static bool FindStateStartingInWord(WordList* words,
                                      size_t state_first_word,
                                      size_t* state_last_word,
                                      String16Tokenizer* tokenizer,
                                      size_t* state_index);

  static bool IsValidLocationName(const Word& word);
  static bool IsZipValid(const Word& word, size_t state_index);
  static bool IsZipValidForState(const Word& word, size_t state_index);

  DISALLOW_COPY_AND_ASSIGN(AddressDetector);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_ADDRESS_DETECTOR_H_
