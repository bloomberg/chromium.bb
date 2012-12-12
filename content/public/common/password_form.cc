// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/password_form.h"

namespace content {

PasswordForm::PasswordForm()
    : scheme(SCHEME_HTML),
      password_autocomplete_set(true),
      ssl_valid(false),
      preferred(false),
      blacklisted_by_user(false),
      type(TYPE_MANUAL) {
}

PasswordForm::~PasswordForm() {
}

}  // namespace content
