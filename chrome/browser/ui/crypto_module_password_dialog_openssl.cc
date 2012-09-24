// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/crypto_module_password_dialog.h"

#include "base/logging.h"

namespace chrome {

void UnlockSlotsIfNecessary(const net::CryptoModuleList& modules,
                            CryptoModulePasswordReason reason,
                            const std::string& host,
                            const base::Closure& callback) {
  // TODO(bulach): implement me.
  NOTREACHED();
}

void UnlockCertSlotIfNecessary(net::X509Certificate* cert,
                               CryptoModulePasswordReason reason,
                               const std::string& host,
                               const base::Closure& callback) {
  // TODO(bulach): implement me.
  NOTREACHED();
}

}  // namespace chrome
