// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_TEXT_INPUT_INFO_H_
#define CHROME_BROWSER_VR_MODEL_TEXT_INPUT_INFO_H_

#include "base/strings/string16.h"

namespace vr {

// Represents the state of an editable text field.
struct TextInputInfo {
 public:
  TextInputInfo();
  explicit TextInputInfo(base::string16 t);

  bool operator==(const TextInputInfo& other) const;
  bool operator!=(const TextInputInfo& other) const;

  // The value of the input field.
  base::string16 text;

  // The cursor position of the current selection start, or the caret position
  // if nothing is selected.
  int selection_start;

  // The cursor position of the current selection end, or the caret position
  // if nothing is selected.
  int selection_end;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_TEXT_INPUT_INFO_H_
