// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/user.h"

namespace chromeos {

void LoginStatusConsumer::OnRetailModeLoginSuccess(
    const UserContext& user_context) {
  OnLoginSuccess(user_context,
                 false,   // pending_requests
                 false);  // using_oauth
}

void LoginStatusConsumer::OnPasswordChangeDetected() {
  NOTREACHED();
}

}  // namespace chromeos
