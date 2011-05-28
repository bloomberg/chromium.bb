// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_scanner.h"

#include "base/logging.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "unicode/regex.h"

AutofillScanner::AutofillScanner(
    const std::vector<const AutofillField*>& fields)
    : cursor_(fields.begin()),
      end_(fields.end()) {
}

AutofillScanner::~AutofillScanner() {
}

void AutofillScanner::Advance() {
  DCHECK(!IsEnd());
  ++cursor_;
}

const AutofillField* AutofillScanner::Cursor() const {
  if (IsEnd()) {
    NOTREACHED();
    return NULL;
  }

  return *cursor_;
}

bool AutofillScanner::IsEnd() const {
  return cursor_ == end_;
}

void AutofillScanner::Rewind() {
  DCHECK(!saved_cursors_.empty());
  cursor_ = saved_cursors_.back();
  saved_cursors_.pop_back();
}

void AutofillScanner::SaveCursor() {
  saved_cursors_.push_back(cursor_);
}

namespace autofill {

bool MatchString(const string16& input, const string16& pattern) {
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString icu_pattern(pattern.data(), pattern.length());
  icu::UnicodeString icu_input(input.data(), input.length());
  icu::RegexMatcher matcher(icu_pattern, icu_input,
                            UREGEX_CASE_INSENSITIVE, status);
  DCHECK(U_SUCCESS(status));

  UBool match = matcher.find(0, status);
  DCHECK(U_SUCCESS(status));
  return !!match;
}

}  // namespace autofill

