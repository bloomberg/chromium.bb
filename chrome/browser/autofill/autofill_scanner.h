// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_SCANNER_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_SCANNER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"

class AutofillField;

// A helper class for parsing a stream of |AutofillField|'s with lookahead.
class AutofillScanner {
 public:
  explicit AutofillScanner(const std::vector<const AutofillField*>& fields);
  ~AutofillScanner();

  // Advances the cursor by one step, if possible.
  void Advance();

  // Returns the current field in the stream, or |NULL| if there are no more
  // fields in the stream.
  const AutofillField* Cursor() const;

  // Returns |true| if the cursor has reached the end of the stream.
  bool IsEnd() const;

  // Returns the most recently saved cursor -- see also |SaveCursor()|.
  void Rewind();

  // Saves the current cursor position.  Multiple cursor positions can be saved,
  // with stack ordering semantics.  See also |Rewind()|.
  void SaveCursor();

 private:
  // Indicates the current position in the stream, represented as a vector.
  std::vector<const AutofillField*>::const_iterator cursor_;

  // A stack of saved positions in the stream.
  std::vector<std::vector<const AutofillField*>::const_iterator> saved_cursors_;

  // The past-the-end pointer for the stream.
  const std::vector<const AutofillField*>::const_iterator end_;

  DISALLOW_COPY_AND_ASSIGN(AutofillScanner);
};

// Parsing utilities.
namespace autofill {

// Case-insensitive regular expression matching.  Returns true if |pattern| is
// found in |input|.
bool MatchString(const string16& input, const string16& pattern);

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_SCANNER_H_
