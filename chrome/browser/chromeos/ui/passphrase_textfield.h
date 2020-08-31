// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_PASSPHRASE_TEXTFIELD_H_
#define CHROME_BROWSER_CHROMEOS_UI_PASSPHRASE_TEXTFIELD_H_

#include <string>

#include "ui/views/controls/textfield/textfield.h"

namespace chromeos {

class PassphraseTextfield : public views::Textfield {
 public:
  PassphraseTextfield();

  // If show_fake is true, then the text field will show a fake password.
  void SetShowFake(bool show_fake);

  // Override views::Textfield so that when focus is gained, then clear out the
  // fake password if appropriate. Replace it when focus is lost if the user has
  // not typed in a new password.
  void OnFocus() override;
  void OnBlur() override;

  // Returns the passphrase. If it's unchanged, then returns an empty string.
  std::string GetPassphrase();

  bool changed() const { return changed_; }
  bool show_fake() const { return show_fake_; }

 private:
  void SetFakePassphrase();
  void ClearFakePassphrase();

  bool show_fake_;
  bool changed_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_PASSPHRASE_TEXTFIELD_H_
