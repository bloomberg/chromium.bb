// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/new_password_form_manager.h"

using autofill::FormData;

namespace password_manager {

NewPasswordFormManager::NewPasswordFormManager(
    const autofill::FormData& observed_form)
    : observed_form_(observed_form) {}
NewPasswordFormManager::~NewPasswordFormManager() = default;

bool NewPasswordFormManager::DoesManage(const autofill::FormData& form) const {
  return observed_form_.SameFormAs(form);
}

}  // namespace password_manager
