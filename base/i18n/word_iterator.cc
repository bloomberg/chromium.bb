// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/word_iterator.h"

#include "base/logging.h"
#include "unicode/ubrk.h"
#include "unicode/ustring.h"

const size_t npos = -1;

WordIterator::WordIterator(const string16* str, BreakType break_type)
    : iter_(NULL),
      string_(str),
      break_type_(break_type),
      prev_(npos),
      pos_(0) {
}

WordIterator::~WordIterator() {
  if (iter_)
    ubrk_close(iter_);
}

bool WordIterator::Init() {
  UErrorCode status = U_ZERO_ERROR;
  UBreakIteratorType break_type;
  switch (break_type_) {
    case BREAK_WORD:
      break_type = UBRK_WORD;
      break;
    case BREAK_LINE:
      break_type = UBRK_LINE;
      break;
    default:
      NOTREACHED();
      break_type = UBRK_LINE;
  }
  iter_ = ubrk_open(break_type, NULL,
                    string_->data(), static_cast<int32_t>(string_->size()),
                    &status);
  if (U_FAILURE(status)) {
    NOTREACHED() << "ubrk_open failed";
    return false;
  }
  ubrk_first(iter_);  // Move the iterator to the beginning of the string.
  return true;
}

bool WordIterator::Advance() {
  prev_ = pos_;
  const int32_t pos = ubrk_next(iter_);
  if (pos == UBRK_DONE) {
    pos_ = npos;
    return false;
  } else {
    pos_ = static_cast<size_t>(pos);
    return true;
  }
}

bool WordIterator::IsWord() const {
  return (ubrk_getRuleStatus(iter_) != UBRK_WORD_NONE);
}

string16 WordIterator::GetWord() const {
  DCHECK(prev_ != npos && pos_ != npos);
  return string_->substr(prev_, pos_ - prev_);
}
