// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/keyboard_edit.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace vr {

KeyboardEdit::KeyboardEdit(KeyboardEditType type) : type_(type) {}
KeyboardEdit::KeyboardEdit(KeyboardEditType type,
                           base::string16 text,
                           int new_cursor_position)
    : type_(type), text_(text), new_cursor_position_(new_cursor_position) {}

bool KeyboardEdit::operator==(const KeyboardEdit& other) const {
  return type_ == other.type() && text_ == other.text() &&
         new_cursor_position_ == other.cursor_position();
}

bool KeyboardEdit::operator!=(const KeyboardEdit& other) const {
  return !(*this == other);
}

std::string KeyboardEdit::ToString() const {
  return base::StringPrintf("type(%d) t(%s) c(%d)", type_,
                            base::UTF16ToUTF8(text_).c_str(),
                            new_cursor_position_);
}

}  // namespace vr
