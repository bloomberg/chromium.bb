// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_BIDI_LINE_ITERATOR_H_
#define APP_BIDI_LINE_ITERATOR_H_

#include <string>

#include "unicode/ubidi.h"

#include "base/basictypes.h"

// A simple wrapper class for the bidirectional iterator of ICU.
// This class uses the bidirectional iterator of ICU to split a line of
// bidirectional texts into visual runs in its display order.
class BiDiLineIterator {
 public:
  BiDiLineIterator() : bidi_(NULL) { }
  ~BiDiLineIterator();

  // Initializes the bidirectional iterator with the specified text.  Returns
  // whether initialization succeeded.
  UBool Open(const std::wstring& text, bool right_to_left, bool url);

  // Returns the number of visual runs in the text, or zero on error.
  int CountRuns();

  // Gets the logical offset, length, and direction of the specified visual run.
  UBiDiDirection GetVisualRun(int index, int* start, int* length);

  // Given a start position, figure out where the run ends (and the BiDiLevel).
  void GetLogicalRun(int start, int* end, UBiDiLevel* level);

 private:
  UBiDi* bidi_;

  DISALLOW_COPY_AND_ASSIGN(BiDiLineIterator);
};

#endif  // APP_BIDI_LINE_ITERATOR_H_
