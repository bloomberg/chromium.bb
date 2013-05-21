// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_MISSPELLING_H_
#define CHROME_BROWSER_SPELLCHECKER_MISSPELLING_H_

#include <vector>

#include "base/time.h"
#include "chrome/browser/spellchecker/spellcheck_action.h"

// Spellcheck misspelling.
class Misspelling {
 public:
  Misspelling();
  Misspelling(const string16& context,
              size_t location,
              size_t length,
              const std::vector<string16>& suggestions,
              uint32 hash);
  ~Misspelling();

  // Serializes the data in this object into a dictionary value. The caller owns
  // the result.
  base::DictionaryValue* Serialize() const;

  // A several-word text snippet that immediately surrounds the misspelling.
  string16 context;

  // The number of characters between the beginning of |context| and the first
  // misspelled character.
  size_t location;

  // The number of characters in the misspelling.
  size_t length;

  // Spelling suggestions.
  std::vector<string16> suggestions;

  // The hash that identifies the misspelling.
  uint32 hash;

  // User action.
  SpellcheckAction action;

  // The time when the user applied the action.
  base::Time timestamp;
};

#endif  // CHROME_BROWSER_SPELLCHECKER_MISSPELLING_H_
