// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_TEXT_INPUT_INFO_H_
#define CHROME_BROWSER_VR_MODEL_TEXT_INPUT_INFO_H_

#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace vr {

// Represents the state of an editable text field.
struct TextInputInfo {
 public:
  TextInputInfo();
  explicit TextInputInfo(base::string16 t);

  static const int kDefaultCompositionIndex = -1;

  bool operator==(const TextInputInfo& other) const;
  bool operator!=(const TextInputInfo& other) const;

  size_t SelectionSize() const;

  // The value of the input field.
  base::string16 text;

  // The cursor position of the current selection start, or the caret position
  // if nothing is selected.
  int selection_start;

  // The cursor position of the current selection end, or the caret position
  // if nothing is selected.
  int selection_end;

  // The start position of the current composition, or -1 if there is none.
  int composition_start;

  // The end position of the current composition, or -1 if there is none.
  int composition_end;

  std::string ToString() const {
    return base::StringPrintf(
        "t(%s) s(%d, %d) c(%d, %d)", base::UTF16ToUTF8(text).c_str(),
        selection_start, selection_end, composition_start, composition_end);
  }
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_TEXT_INPUT_INFO_H_
