// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An object to store user feedback to a single spellcheck suggestion.
//
// Stores the spellcheck suggestion, its uint32 hash identifier, and user's
// feedback. The feedback is indirect, in the sense that we record user's
// |action| instead of asking them how they feel about a spellcheck suggestion.
// The object can serialize itself.

#ifndef CHROME_BROWSER_SPELLCHECKER_MISSPELLING_H_
#define CHROME_BROWSER_SPELLCHECKER_MISSPELLING_H_

#include <vector>

#include "base/time/time.h"
#include "chrome/browser/spellchecker/spellcheck_action.h"

// Stores user feedback to a spellcheck suggestion. Sample usage:
//    Misspelling misspelling.
//    misspelling.context = base::ASCIIToUTF16("Helllo world");
//    misspelling.location = 0;
//    misspelling.length = 6;
//    misspelling.suggestions =
//        std::vector<base::string16>(1, base::ASCIIToUTF16("Hello"));
//    misspelling.hash = GenerateRandomHash();
//    misspelling.action.type = SpellcheckAction::TYPE_SELECT;
//    misspelling.action.index = 0;
//    Process(misspelling.Serialize());
class Misspelling {
 public:
  Misspelling();
  Misspelling(const base::string16& context,
              size_t location,
              size_t length,
              const std::vector<base::string16>& suggestions,
              uint32 hash);
  ~Misspelling();

  // Serializes the data in this object into a dictionary value. The caller owns
  // the result.
  base::DictionaryValue* Serialize() const;

  // Returns the substring of |context| that begins at |location| and contains
  // |length| characters.
  base::string16 GetMisspelledString() const;

  // A several-word text snippet that immediately surrounds the misspelling.
  base::string16 context;

  // The number of characters between the beginning of |context| and the first
  // misspelled character.
  size_t location;

  // The number of characters in the misspelling.
  size_t length;

  // Spelling suggestions.
  std::vector<base::string16> suggestions;

  // The hash that identifies the misspelling.
  uint32 hash;

  // User action.
  SpellcheckAction action;

  // The time when the user applied the action.
  base::Time timestamp;
};

#endif  // CHROME_BROWSER_SPELLCHECKER_MISSPELLING_H_
