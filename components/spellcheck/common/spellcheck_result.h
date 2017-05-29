// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_RESULT_H_
#define COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_RESULT_H_

#include <stdint.h>

#include "base/strings/string16.h"

// This class mirrors blink::WebTextCheckingResult which holds a
// misspelled range inside the checked text. It also contains a
// possible replacement of the misspelling if it is available.
struct SpellCheckResult {
  enum Decoration {
    // Red underline for misspelled words.
    SPELLING = 1 << 1,

    // Gray underline for correctly spelled words that are incorrectly used in
    // their context.
    GRAMMAR = 1 << 2,
  };

  explicit SpellCheckResult(Decoration d = SPELLING,
                            int loc = 0,
                            int len = 0,
                            const base::string16& rep = base::string16())
      : decoration(d), location(loc), length(len), replacement(rep) {}

  Decoration decoration;
  int location;
  int length;
  base::string16 replacement;
};

#endif  // COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_RESULT_H_
