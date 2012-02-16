// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/passphrase_textfield.h"

#include "base/utf_string_conversions.h"

namespace chromeos {

// String to use as password if the password is set but not available in the UI.
// User will see this as ********
const string16 kFakePassphrase = ASCIIToUTF16("********");

PassphraseTextfield::PassphraseTextfield(bool show_fake)
    : Textfield(views::Textfield::STYLE_OBSCURED),
      show_fake_(show_fake),
      changed_(true) {
  if (show_fake_) {
    SetText(kFakePassphrase);
    changed_ = false;
  }
}

void PassphraseTextfield::OnFocus() {
  // If showing the fake password, then clear it when focused.
  if (show_fake_ && !changed_) {
    SetText(string16());
    changed_ = true;
  }
}

void PassphraseTextfield::OnBlur() {
  // If passowrd is not changed, then show the fake password when blurred.
  if (show_fake_ && text().empty()) {
    SetText(kFakePassphrase);
    changed_ = false;
  }
}

std::string PassphraseTextfield::GetPassphrase() {
  return changed_ ? UTF16ToUTF8(text()) : std::string();
}

}  // namespace chromeos
