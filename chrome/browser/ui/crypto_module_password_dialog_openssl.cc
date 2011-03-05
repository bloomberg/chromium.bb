// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/crypto_module_password_dialog.h"

#include "base/logging.h"

namespace browser {

void UnlockSlotsIfNecessary(const net::CryptoModuleList& modules,
                            browser::CryptoModulePasswordReason reason,
                            const std::string& host,
                            Callback0::Type* callback) {
  // TODO(bulach): implement me.
  NOTREACHED();
}

void UnlockCertSlotIfNecessary(net::X509Certificate* cert,
                               browser::CryptoModulePasswordReason reason,
                               const std::string& host,
                               Callback0::Type* callback) {
  // TODO(bulach): implement me.
  NOTREACHED();
}

}  // namespace browser
