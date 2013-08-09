// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/passphrase_textfield.h"

#include "base/strings/utf_string_conversions.h"

namespace chromeos {

PassphraseTextfield::PassphraseTextfield()
    : Textfield(views::Textfield::STYLE_OBSCURED),
      show_fake_(false),
      changed_(true) {
}

void PassphraseTextfield::SetShowFake(bool show_fake) {
  show_fake_ = show_fake;
  if (show_fake_)
    SetFakePassphrase();
  else
    ClearFakePassphrase();
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
