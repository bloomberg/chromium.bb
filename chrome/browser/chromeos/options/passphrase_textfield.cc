// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/passphrase_textfield.h"

#include "base/utf_string_conversions.h"

namespace chromeos {

PassphraseTextfield::PassphraseTextfield(bool show_fake)
    : Textfield(views::Textfield::STYLE_OBSCURED),
      show_fake_(show_fake),
      changed_(true) {
  if (show_fake_)
    SetFakePassphrase();
}

void PassphraseTextfield::OnFocus() {
  // If showing the fake password, then clear it when focused.
  if (show_fake_ && !changed_)
    ClearFakePassphrase();
  Textfield::OnFocus();
}

void PassphraseTextfield::OnBlur() {
  // If password is not changed, then show the fake password when blurred.
  if (show_fake_ && text().empty())
    SetFakePassphrase();
  Textfield::OnBlur();
}

std::string PassphraseTextfield::GetPassphrase() {
  return changed_ ? UTF16ToUTF8(text()) : std::string();
}

void PassphraseTextfield::SetFakePassphrase() {
  CR_DEFINE_STATIC_LOCAL(string16, fake_passphrase, (ASCIIToUTF16("********")));
  SetText(fake_passphrase);
  changed_ = false;
}

void PassphraseTextfield::ClearFakePassphrase() {
  SetText(string16());
  changed_ = true;
}

}  // namespace chromeos
