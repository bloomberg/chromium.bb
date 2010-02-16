// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_WORDITERATOR_H_
#define CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_WORDITERATOR_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/string16.h"
#include "third_party/icu/public/common/unicode/ubrk.h"
#include "third_party/icu/public/common/unicode/uscript.h"

// A class which encapsulates language-specific operations used by
// SpellcheckWordIterator.
// When we set the spellchecker language, this class creates rule sets that
// filter out the characters not supported by the spellchecker.
// (Please read the comment in the SpellcheckWordIterator class about how to
// use this class.)
class SpellcheckCharAttribute {
 public:
  SpellcheckCharAttribute();
  ~SpellcheckCharAttribute();

  // Sets the language of the spellchecker.
  // This function creates the custom rule-sets used by SpellcheckWordIterator.
  // Parameters
  //   * language [in] (std::string)
  //     The language-code string.
  void SetDefaultLanguage(const std::string& language);

  // Returns a custom rule-set string used by the ICU break iterator.
  // Parameters
  //   * allow_contraction [in] (bool)
  //     A flag to control whether or not this object splits a possible
  //     contraction. If this value is false, it returns a rule set that
  //    splits a possible contraction: "in'n'out" -> "in", "n", and "out".
  string16 GetRuleSet(bool allow_contraction) const;

  // Output a character only if it is a word character.
  bool OutputChar(UChar c, string16* output) const;

 private:
  // Creates the rule-set strings.
  void CreateRuleSets(const std::string& language);

  // Language-specific output functions.
  bool OutputArabic(UChar c, string16* output) const;
  bool OutputHangul(UChar c, string16* output) const;
  bool OutputHebrew(UChar c, string16* output) const;
  bool OutputDefault(UChar c, string16* output) const;

 private:
  // The custom rule-set strings used by ICU BreakIterator.
  // Since it is not so easy to create custom rule-sets from a spellchecker
  // language, this class saves these rule-set strings created when we set the
  // language.
  string16 ruleset_allow_contraction_;
  string16 ruleset_disallow_contraction_;

  // The script code used by this language.
  UScriptCode script_code_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckCharAttribute);
};

// A class which extracts words that can be checked for spelling from a longer
// string.
// The ICU word-break iterator does not discard some punctuation characters
// attached to a word. For example, when we set a word "_hello_" to a
// word-break iterator, it just returns "_hello_".
// On the other hand, our spellchecker expects for us to discard such
// punctuation characters.
// To extract only the words that our spellchecker can check, this class uses
// custom rule-sets created by the SpellcheckCharAttribute class.
// Also, this class normalizes extracted words so our spellchecker can check
// the spellings of a word that includes ligatures, combined characters,
// full-width characters, etc.
//
// The following snippet is an example that extracts words with this class.
//
//   // Creates the language-specific attributes for US English.
//   SpellcheckCharAttribute attribute;
//   attribute.SetDefaultLanguage("en-US");
//
//   // Set up a SpellcheckWordIterator object which extracts English words,
//   // and retrieves them.
//   SpellcheckWordIterator iterator;
//   string16 text(UTF8ToUTF16("this is a test."));
//   iterator.Initialize(&attribute, text.c_str(), text_.length(), true);
//
//   string16 word;
//   int start;
//   int end;
//   while (iterator.GetNextWord(&word, &start, &end)) {
//     ...
//   }
//
class SpellcheckWordIterator {
 public:
  SpellcheckWordIterator();
  ~SpellcheckWordIterator();

  // Initializes a word-iterator object.
  // Parameters
  //   * attribute [in] (const SpellcheckCharAttribute*)
  //     Character attributes used for filtering out non-word characters.
  //   * word [in] (const char16*)
  //     A string from which this object extracts words. (This string does not
  //     have to be NUL-terminated.)
  //   * length [in] (size_t)
  //     The length of the given string, in UTF-16 characters.
  //   * allow_contraction [in] (bool)
  //     A flag to control whether or not this object should split a possible
  //     contraction (e.g. "isn't", "in'n'out", etc.)
  // Return values
  //   * true
  //     This word-iterator object is initialized successfully.
  //   * false
  //     An error occured while initializing this object.
  bool Initialize(const SpellcheckCharAttribute* attribute,
                  const char16* word,
                  size_t length,
                  bool allow_contraction);

  // Retrieves a word (or a contraction).
  // Parameters
  //   * word_string [out] (string16*)
  //     A word (or a contraction) to be checked its spelling. This
  //     |word_string| has been already normalized to its canonical form (i.e.
  //     decomposed ligatures, replaced full-width latin characters to its ASCII
  //     alternatives, etc.) so a SpellChecker object can check its spelling
  //     without any additional operations. We can use |word_start| and
  //     |word_length| to retrieve the non-normalizedversion of this string as
  //     shown in the following snippet.
  //       string16 str(&word[word_start], word_length);
  //   * word_start [out] (int*)
  //     The offset of this word from the beginning of the input string,
  //     in UTF-16 characters.
  //   * word_length [out] (int*)
  //     The length of an extracted word before normalization, in UTF-16
  //     characters.
  //     When the input string contains ligatures, this value may not be equal
  //     to the length of the |word_string|.
  // Return values
  //   * true
  //     Found a word (or a contraction) to be checked its spelling.
  //   * false
  //     Not found any more words or contractions to be checked their spellings.
  bool GetNextWord(string16* word_string,
                   int* word_start,
                   int* word_length);

 private:
  // Releases all the resources attached to this object.
  void Close();

  // Normalizes a non-terminated string so our spellchecker can check its
  // spelling. A word returned from an ICU word-break iterator may include
  // characters not supported by our spellchecker, e.g. ligatures, combining
  // characters, full-width letters, etc. This function replaces such characters
  // with alternative characters supported by our spellchecker.
  bool Normalize(int input_start,
                 int input_length,
                 string16* output_string) const;

 private:
  // The pointer to the input string from which we are extracting words.
  const char16* word_;

  // The length of the original string.
  int length_;

  // The current position in the original string.
  int position_;

  // The language-specific attributes used for filtering out non-word
  // characters.
  const SpellcheckCharAttribute* attribute_;

  // The ICU break iterator.
  UBreakIterator* iterator_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckWordIterator);
};

#endif  // CHROME_RENDERER_SPELLCHECKER_SPELLCHECK_WORDITERATOR_H_
