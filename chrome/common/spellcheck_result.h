// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SPELLCHECK_RESULT_H_
#define CHROME_COMMON_SPELLCHECK_RESULT_H_

#include "base/strings/string16.h"

// This class mirrors WebKit::WebTextCheckingResult which holds a
// misspelled range inside the checked text. It also contains a
// possible replacement of the misspelling if it is available.
//
// Although SpellCheckResult::Type defines various values Chromium
// only uses the |Spelling| and |Grammar| types.
//
struct SpellCheckResult {
  enum Type {
    SPELLING = 1 << 1,
    GRAMMAR  = 1 << 2,
  };

  explicit SpellCheckResult(
      Type t = SPELLING,
      int loc = 0,
      int len = 0,
      const string16& rep = string16(),
      uint32 h = 0)
      : type(t), location(loc), length(len), replacement(rep), hash(h) {
  }

  Type type;
  int location;
  int length;
  string16 replacement;
  uint32 hash;
};

#endif  // CHROME_COMMON_SPELLCHECK_RESULT_H_
