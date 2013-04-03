// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/tokenized_string.h"

#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "chrome/browser/ui/app_list/search/term_break_iterator.h"

using base::i18n::BreakIterator;

namespace app_list {

TokenizedString::TokenizedString(const string16& text)
    : text_(text) {
  Tokenize();
}

TokenizedString::~TokenizedString() {}

void TokenizedString::Tokenize() {
  BreakIterator break_iter(text_,  BreakIterator::BREAK_WORD);
  CHECK(break_iter.Init());

  while (break_iter.Advance()) {
    if (!break_iter.IsWord())
      continue;

    const string16 word(break_iter.GetString());
    const size_t word_start = break_iter.prev();
    TermBreakIterator term_iter(word);
    while (term_iter.Advance()) {
      tokens_.push_back(base::i18n::ToLower(term_iter.GetCurrentTerm()));
      mappings_.push_back(ui::Range(word_start + term_iter.prev(),
                                    word_start + term_iter.pos()));
    }
  }
}

}  // namespace app_list
