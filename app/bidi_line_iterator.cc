// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/bidi_line_iterator.h"

#include "base/logging.h"
#include "base/string16.h"
#include "base/string_util.h"

BiDiLineIterator::~BiDiLineIterator() {
  if (bidi_) {
    ubidi_close(bidi_);
    bidi_ = NULL;
  }
}

UBool BiDiLineIterator::Open(const std::wstring& text,
                             bool right_to_left,
                             bool url) {
  DCHECK(bidi_ == NULL);
  UErrorCode error = U_ZERO_ERROR;
  bidi_ = ubidi_openSized(static_cast<int>(text.length()), 0, &error);
  if (U_FAILURE(error))
    return false;
  if (right_to_left && url)
    ubidi_setReorderingMode(bidi_, UBIDI_REORDER_RUNS_ONLY);
#if defined(WCHAR_T_IS_UTF32)
  const string16 text_utf16 = WideToUTF16(text);
#else
  const std::wstring &text_utf16 = text;
#endif  // U_SIZEOF_WCHAR_T != 4
  ubidi_setPara(bidi_, text_utf16.data(), static_cast<int>(text_utf16.length()),
                right_to_left ? UBIDI_DEFAULT_RTL : UBIDI_DEFAULT_LTR,
                NULL, &error);
  return U_SUCCESS(error);
}

int BiDiLineIterator::CountRuns() {
  DCHECK(bidi_ != NULL);
  UErrorCode error = U_ZERO_ERROR;
  const int runs = ubidi_countRuns(bidi_, &error);
  return U_SUCCESS(error) ? runs : 0;
}

UBiDiDirection BiDiLineIterator::GetVisualRun(int index,
                                              int* start,
                                              int* length) {
  DCHECK(bidi_ != NULL);
  return ubidi_getVisualRun(bidi_, index, start, length);
}

void BiDiLineIterator::GetLogicalRun(int start,
                                     int* end,
                                     UBiDiLevel* level) {
  DCHECK(bidi_ != NULL);
  ubidi_getLogicalRun(bidi_, start, end, level);
}
