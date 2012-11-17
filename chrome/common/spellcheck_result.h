// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SPELLCHECK_RESULT_H_
#define CHROME_COMMON_SPELLCHECK_RESULT_H_

#include "base/string16.h"

// This class mirrors WebKit::WebTextCheckingResult which holds a
// misspelled range inside the checked text. It also contains a
// possible replacement of the misspelling if it is available.
//
// Although SpellCheckResult::Type defines various values Chromium
// only uses the |Spelling| type. Other values are just reflecting the
// enum definition in the original WebKit class.
//
struct SpellCheckResult {
  enum Type {
    SPELLING = 1 << 1,
    GRAMMAR  = 1 << 2,
    LINK = 1 << 5,
    QUOTE = 1 << 6,
    DASH = 1 << 7,
    REPLACEMENT = 1 << 8,
    CORRECTION = 1 << 9,
    SHOWCORRECTIONPANEL = 1 << 10
  };

  explicit SpellCheckResult(
      Type t = SPELLING,
      int loc = 0,
      int len = 0,
      const string16& rep = string16())
      : type(t), location(loc), length(len), replacement(rep) {
  }

  Type type;
  int location;
  int length;
  string16 replacement;
};

#endif  // CHROME_COMMON_SPELLCHECK_RESULT_H_
