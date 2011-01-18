// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pk11_password_dialog.h"

#include "base/logging.h"

namespace browser {

void UnlockSlotIfNecessary(net::CryptoModule* module,
                           browser::PK11PasswordReason reason,
                           const std::string& host,
                           Callback0::Type* callback) {
  // TODO(bulach): implement me.
  NOTREACHED();
}

void UnlockCertSlotIfNecessary(net::X509Certificate* cert,
                               browser::PK11PasswordReason reason,
                               const std::string& host,
                               Callback0::Type* callback) {
  // TODO(bulach): implement me.
  NOTREACHED();
}

}  // namespace browser
