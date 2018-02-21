// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_KEYBOARD_EDIT_H_
#define CHROME_BROWSER_VR_KEYBOARD_EDIT_H_

#include "base/strings/string16.h"

namespace vr {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr_shell
enum KeyboardEditType {
  COMMIT_TEXT,
  DELETE_TEXT,
  SUBMIT,
};

class KeyboardEdit {
 public:
  explicit KeyboardEdit(KeyboardEditType type);
  KeyboardEdit(KeyboardEditType type,
               base::string16 text,
               int new_cursor_position);

  KeyboardEditType type() const { return type_; }
  base::string16 text() const { return text_; }
  int cursor_position() const { return new_cursor_position_; }

  bool operator==(const KeyboardEdit& other) const;
  bool operator!=(const KeyboardEdit& other) const;

  std::string ToString() const;

 private:
  KeyboardEditType type_;
  base::string16 text_;
  int new_cursor_position_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_KEYBOARD_EDIT_H_
