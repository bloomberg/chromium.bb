// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/tailored_word_break_iterator.h"

namespace {
constexpr base::char16 kUnderscore = '_';
}  // namespace

using base::i18n::BreakIterator;

TailoredWordBreakIterator::TailoredWordBreakIterator(
    const base::StringPiece16& str,
    BreakIterator::BreakType break_type)
    : BreakIterator(str, break_type), prev_(0), pos_(0) {
  DCHECK_EQ(BreakIterator::BREAK_WORD, break_type);
}

TailoredWordBreakIterator::~TailoredWordBreakIterator() {}

bool TailoredWordBreakIterator::Advance() {
  if (HasUnderscoreWord()) {
    AdvanceInUnderscoreWord();
    return true;
  }
  if (!BreakIterator::Advance())
    return false;
  prev_ = 0;
  pos_ = 0;
  underscore_word_ = base::StringPiece16();
  if (!IsWord())
    return true;
  base::StringPiece16 word = BreakIterator::GetStringPiece();
  if (word.find(kUnderscore) != base::StringPiece16::npos) {
    underscore_word_ = word;
    AdvanceInUnderscoreWord();
  }
  return true;
}

bool TailoredWordBreakIterator::IsWord() const {
  if (HasUnderscoreWord()) {
    base::StringPiece16 word = GetStringPiece();
    if (!word.empty())
      return word[0] != kUnderscore;
  }
  return BreakIterator::IsWord();
}

base::StringPiece16 TailoredWordBreakIterator::GetStringPiece() const {
  if (!underscore_word_.empty())
    return underscore_word_.substr(prev_, pos_ - prev_);
  return BreakIterator::GetStringPiece();
}

base::string16 TailoredWordBreakIterator::GetString() const {
  return GetStringPiece().as_string();
}

size_t TailoredWordBreakIterator::prev() const {
  return BreakIterator::prev() + prev_;
}

size_t TailoredWordBreakIterator::pos() const {
  return BreakIterator::pos() + pos_;
}

bool TailoredWordBreakIterator::HasUnderscoreWord() const {
  return pos_ != underscore_word_.size();
}

void TailoredWordBreakIterator::AdvanceInUnderscoreWord() {
  std::size_t next_pos = underscore_word_.find(kUnderscore, pos_);
  prev_ = pos_;
  if (next_pos == base::StringPiece16::npos) {
    pos_ = underscore_word_.size();
    return;
  }
  // If an underscore is found at the current position, index moves to next
  // char.
  if (pos_ == next_pos)
    pos_ += 1;
  else
    pos_ = next_pos;
}
