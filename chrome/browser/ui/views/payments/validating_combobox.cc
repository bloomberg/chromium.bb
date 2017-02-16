// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/validating_combobox.h"

#include <utility>

namespace payments {

ValidatingCombobox::ValidatingCombobox(
    std::unique_ptr<ui::ComboboxModel> model,
    std::unique_ptr<ValidationDelegate> delegate)
    : Combobox(std::move(model)),
      delegate_(std::move(delegate)),
      was_blurred_(false) {}

ValidatingCombobox::~ValidatingCombobox() {}

void ValidatingCombobox::OnBlur() {
  Combobox::OnBlur();

  // The first validation should be on a blur. The subsequent validations will
  // occur when the content changes.
  if (!was_blurred_) {
    was_blurred_ = true;
    Validate();
  }
}

void ValidatingCombobox::OnContentsChanged() {
  // Validation on every keystroke only happens if the field has been validated
  // before as part of a blur.
  if (!was_blurred_)
    return;

  Validate();
}

void ValidatingCombobox::Validate() {
  // ValidateCombobox may have side-effects, such as displaying errors.
  SetInvalid(!delegate_->ValidateCombobox(this));
}

}  // namespace payments
