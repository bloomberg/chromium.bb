// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_AUTHENTICATOR_H_
#define DEVICE_FIDO_WIN_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/atomic_flag.h"
#include "base/threading/thread.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/win/webauthn_api.h"

namespace device {

// WinWebAuthnApiAuthenticator forwards WebAuthn requests to external
// authenticators via the native Windows WebAuthentication API
// (webauthn.dll).
class COMPONENT_EXPORT(DEVICE_FIDO) WinWebAuthnApiAuthenticator
    : public FidoAuthenticator {
 public:
  // The return value of |GetId|.
  static const char kAuthenticatorId[];

  WinWebAuthnApiAuthenticator(WinWebAuthnApi* win_api, HWND current_window);
  ~WinWebAuthnApiAuthenticator() override;

  // FidoAuthenticator
  void InitializeAuthenticator(base::OnceClosure callback) override;
  void MakeCredential(CtapMakeCredentialRequest request,
                      MakeCredentialCallback callback) override;
  void GetAssertion(CtapGetAssertionRequest request,
                    GetAssertionCallback callback) override;
  void Cancel() override;
  std::string GetId() const override;
  base::string16 GetDisplayName() const override;
  bool IsInPairingMode() const override;
  bool IsPaired() const override;
  const base::Optional<AuthenticatorSupportedOptions>& Options() const override;
  base::Optional<FidoTransportProtocol> AuthenticatorTransport() const override;
  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

 private:
  void MakeCredentialBlocking(
      CtapMakeCredentialRequest request,
      MakeCredentialCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_runner);
  void GetAssertionBlocking(
      CtapGetAssertionRequest request,
      GetAssertionCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_runner);

  void InvokeMakeCredentialCallback(
      MakeCredentialCallback cb,
      CtapDeviceResponseCode status,
      base::Optional<AuthenticatorMakeCredentialResponse> response);
  void InvokeGetAssertionCallback(
      GetAssertionCallback cb,
      CtapDeviceResponseCode status,
      base::Optional<AuthenticatorGetAssertionResponse> response);

  WinWebAuthnApi* win_api_;
  std::unique_ptr<base::Thread> thread_;
  HWND current_window_;

  GUID cancellation_id_ = {};
  base::AtomicFlag operation_cancelled_;
  base::WeakPtrFactory<WinWebAuthnApiAuthenticator> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(WinWebAuthnApiAuthenticator);
};

}  // namespace device

#endif  // DEVICE_FIDO_WIN_AUTHENTICATOR_H_
