// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/recovery_private/error_messages.h"
#include "chrome/browser/extensions/api/recovery_private/recovery_operation.h"

namespace extensions {
namespace recovery {

void RecoveryOperation::WriteStart() {
  Error(error::kUnsupportedOperation);
}

void RecoveryOperation::VerifyWriteStart() {
  Error(error::kUnsupportedOperation);
}

} // namespace recovery
} // namespace extensions
