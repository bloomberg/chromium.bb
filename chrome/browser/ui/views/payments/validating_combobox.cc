// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/validating_combobox.h"

#include <utility>

#include "ui/base/models/combobox_model.h"

namespace payments {

ValidatingCombobox::ValidatingCombobox(
    std::unique_ptr<ui::ComboboxModel> model,
    std::unique_ptr<ValidationDelegate> delegate)
    : Combobox(std::move(model)), delegate_(std::move(delegate)) {
  // No need to remove observer on owned model.
  this->model()->AddObserver(this);
}

ValidatingCombobox::~ValidatingCombobox() {}

void ValidatingCombobox::OnBlur() {
  Combobox::OnBlur();

  // The first validation should be on a blur. The subsequent validations will
  // occur when the content changes. Do not validate if the view is being
  // removed.
  if (!was_blurred_ && !being_removed_) {
    was_blurred_ = true;
    Validate();
  }
}

void ValidatingCombobox::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.child == this && !details.is_add)
    being_removed_ = true;
}

void ValidatingCombobox::OnContentsChanged() {
  // Validation on every keystroke only happens if the field has been validated
  // before as part of a blur.
  if (!was_blurred_)
    return;

  Validate();
}

void ValidatingCombobox::OnComboboxModelChanged(
    ui::ComboboxModel* unused_model) {
  ModelChanged();
  delegate_->ComboboxModelChanged(this);
}

void ValidatingCombobox::Validate() {
  // ValidateCombobox may have side-effects, such as displaying errors.
  SetInvalid(!delegate_->ValidateCombobox(this));
}

}  // namespace payments
