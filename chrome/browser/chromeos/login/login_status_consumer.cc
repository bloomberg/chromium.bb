// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_status_consumer.h"

namespace chromeos {

void LoginStatusConsumer::OnDemoUserLoginSuccess() {
  OnLoginSuccess(kDemoUser, "", false, false);
}

void LoginStatusConsumer::OnPasswordChangeDetected() {
  NOTREACHED();
}

}  // namespace chromeos
