// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_scanner.h"

#include "base/logging.h"
#include "chrome/browser/autofill/autofill_field.h"

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
