// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/text_input_info.h"

namespace vr {

TextInputInfo::TextInputInfo() : selection_start(0), selection_end(0) {}
TextInputInfo::TextInputInfo(base::string16 t)
    : text(t), selection_start(t.length()), selection_end(t.length()) {}

bool TextInputInfo::operator==(const TextInputInfo& other) const {
  return text == other.text && selection_start == other.selection_start &&
         selection_end == other.selection_end;
}

bool TextInputInfo::operator!=(const TextInputInfo& other) const {
  return !(*this == other);
}

static_assert(sizeof(base::string16) + 8 == sizeof(TextInputInfo),
              "If new fields are added to TextInputInfo, we must explicitly "
              "bump this size and update operator==");

}  // namespace vr
