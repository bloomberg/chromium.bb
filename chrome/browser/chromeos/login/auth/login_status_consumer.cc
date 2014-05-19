// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth/login_status_consumer.h"

namespace chromeos {

void LoginStatusConsumer::OnRetailModeLoginSuccess(
    const UserContext& user_context) {
  OnLoginSuccess(user_context);
}

void LoginStatusConsumer::OnPasswordChangeDetected() {
  NOTREACHED();
}

}  // namespace chromeos
