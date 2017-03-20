// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/provisionally_saved_password_form.h"

#include <utility>

#include "components/autofill/core/common/password_form.h"

namespace autofill {

ProvisionallySavedPasswordForm::ProvisionallySavedPasswordForm() = default;

ProvisionallySavedPasswordForm::~ProvisionallySavedPasswordForm() = default;

void ProvisionallySavedPasswordForm::Set(
    std::unique_ptr<PasswordForm> password_form,
    const blink::WebFormElement& form_element,
    const blink::WebInputElement& input_element) {
  password_form_ = std::move(password_form);
  form_element_ = form_element;
  input_element_ = input_element;
}

void ProvisionallySavedPasswordForm::Reset() {
  password_form_.reset();
  form_element_.reset();
  input_element_.reset();
}

bool ProvisionallySavedPasswordForm::IsSet() const {
  return static_cast<bool>(password_form_);
}

bool ProvisionallySavedPasswordForm::IsPasswordValid() const {
  return IsSet() && !password_form_->username_value.empty() &&
         !(password_form_->password_value.empty() &&
           password_form_->new_password_value.empty());
}

}  // namespace autofill
