// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_DISCOVERY_H_
#define DEVICE_FIDO_WIN_DISCOVERY_H_

#include <memory>

#include "base/component_export.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/win/authenticator.h"
#include "device/fido/win/webauthn_api.h"

namespace device {

// Instantiates the authenticator subclass for forwarding requests to external
// authenticators via the Windows WebAuthn API.
class COMPONENT_EXPORT(DEVICE_FIDO) WinWebAuthnApiAuthenticatorDiscovery
    : public FidoDiscoveryBase {
 public:
  WinWebAuthnApiAuthenticatorDiscovery(WinWebAuthnApi* const win_webauthn_api,
                                       HWND parent_window);
  ~WinWebAuthnApiAuthenticatorDiscovery() override;

  // FidoDiscoveryBase:
  void Start() override;

 private:
  std::unique_ptr<WinWebAuthnApiAuthenticator> authenticator_;
  WinWebAuthnApi* const win_webauthn_api_;
  const HWND parent_window_;
};

}  // namespace device

#endif  // DEVICE_FIDO_WIN_DISCOVERY_H_
