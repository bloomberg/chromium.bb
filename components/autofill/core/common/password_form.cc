// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_form.h"

namespace autofill {

PasswordForm::PasswordForm()
    : scheme(SCHEME_HTML),
      password_autocomplete_set(true),
      ssl_valid(false),
      preferred(false),
      blacklisted_by_user(false),
      type(TYPE_MANUAL),
      times_used(0) {
}

PasswordForm::~PasswordForm() {
}

bool PasswordForm::IsPublicSuffixMatch() const {
  return !original_signon_realm.empty();
}

}  // namespace autofill
