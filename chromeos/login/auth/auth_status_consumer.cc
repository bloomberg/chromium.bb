// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/auth_status_consumer.h"

namespace chromeos {

void AuthStatusConsumer::OnRetailModeAuthSuccess(
    const UserContext& user_context) {
  OnAuthSuccess(user_context);
}

void AuthStatusConsumer::OnPasswordChangeDetected() {
  NOTREACHED();
}

}  // namespace chromeos
