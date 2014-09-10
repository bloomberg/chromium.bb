// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/security_wrappers.h"

#include <Security/Security.h>

#include "base/mac/mac_logging.h"

namespace chrome {

ScopedSecKeychainSetUserInteractionAllowed::
    ScopedSecKeychainSetUserInteractionAllowed(Boolean allowed) {
  OSStatus status = SecKeychainGetUserInteractionAllowed(&old_allowed_);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
    old_allowed_ = TRUE;
  }

  status = SecKeychainSetUserInteractionAllowed(allowed);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
  }
}

ScopedSecKeychainSetUserInteractionAllowed::
    ~ScopedSecKeychainSetUserInteractionAllowed() {
  OSStatus status = SecKeychainSetUserInteractionAllowed(old_allowed_);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status);
  }
}

}  // namespace chrome
