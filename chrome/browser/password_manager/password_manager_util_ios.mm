// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_util.h"

namespace password_manager_util {

bool AuthenticateUser(gfx::NativeWindow window) {
  // Per the header comment, since reauthentication is not supported on iOS,
  // this always returns true
  return true;
}

void GetOsPasswordStatus(const base::Callback<void(OsPasswordStatus)>& reply) {
  // No such thing on iOS.
  reply.Run(PASSWORD_STATUS_UNSUPPORTED);
}

}  // namespace password_manager_util
