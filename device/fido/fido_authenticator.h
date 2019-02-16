// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_AUTHENTICATOR_H_
#define DEVICE_FIDO_FIDO_AUTHENTICATOR_H_

#include <string>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/fido_transport_protocol.h"

namespace device {

class CtapGetAssertionRequest;
class CtapMakeCredentialRequest;

namespace pin {
struct RetriesResponse;
struct KeyAgreementResponse;
struct EmptyResponse;
class TokenResponse;
}  // namespace pin

// FidoAuthenticator is an authenticator from the WebAuthn Authenticator model
// (https://www.w3.org/TR/webauthn/#sctn-authenticator-model). It may be a
// physical device, or a built-in (platform) authenticator.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoAuthenticator {
 public:
  using MakeCredentialCallback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      base::Optional<AuthenticatorMakeCredentialResponse>)>;
  using GetAssertionCallback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      base::Optional<AuthenticatorGetAssertionResponse>)>;
  using GetRetriesCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<pin::RetriesResponse>)>;
  using GetEphemeralKeyCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<pin::KeyAgreementResponse>)>;
  using GetPINTokenCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<pin::TokenResponse>)>;
  using SetPINCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<pin::EmptyResponse>)>;
  using ResetCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<pin::EmptyResponse>)>;

  FidoAuthenticator() = default;
  virtual ~FidoAuthenticator() = default;

  // Sends GetInfo request to connected authenticator. Once response to GetInfo
  // call is received, |callback| is invoked. Below MakeCredential() and
  // GetAssertion() must only called after |callback| is invoked.
  virtual void InitializeAuthenticator(base::OnceClosure callback) = 0;
  virtual void MakeCredential(CtapMakeCredentialRequest request,
                              MakeCredentialCallback callback) = 0;
  virtual void GetAssertion(CtapGetAssertionRequest request,
                            GetAssertionCallback callback) = 0;
  // GetTouch causes an (external) authenticator to flash and wait for a touch.
  virtual void GetTouch(base::OnceCallback<void()> callback);
  // GetRetries gets the number of PIN attempts remaining before an
  // authenticator locks. It is only valid to call this method if |Options|
  // indicates that the authenticator supports PINs.
  virtual void GetRetries(GetRetriesCallback callback);
  // GetEphemeralKey fetches an ephemeral P-256 key from the authenticator for
  // use in protecting transmitted PINs. It is only valid to call this method if
  // |Options| indicates that the authenticator supports PINs.
  virtual void GetEphemeralKey(GetEphemeralKeyCallback callback);
  // GetPINToken uses the given PIN to request a PIN-token from an
  // authenticator. It is only valid to call this method if |Options| indicates
  // that the authenticator supports PINs.
  virtual void GetPINToken(std::string pin,
                           const pin::KeyAgreementResponse& peer_key,
                           GetPINTokenCallback callback);
  // SetPIN sets a new PIN on a device that does not currently have one. The
  // length of |pin| must respect |pin::kMinLength| and |pin::kMaxLength|. It is
  // only valid to call this method if |Options| indicates that the
  // authenticator supports PINs.
  virtual void SetPIN(const std::string& pin,
                      pin::KeyAgreementResponse& peer_key,
                      SetPINCallback callback);
  // ChangePIN alters the PIN on a device that already has a PIN set. The
  // length of |pin| must respect |pin::kMinLength| and |pin::kMaxLength|. It is
  // only valid to call this method if |Options| indicates that the
  // authenticator supports PINs.
  virtual void ChangePIN(const std::string& old_pin,
                         const std::string& new_pin,
                         pin::KeyAgreementResponse& peer_key,
                         SetPINCallback callback);
  // Reset triggers a reset operation on the authenticator. This erases all
  // stored resident keys and any configured PIN.
  virtual void Reset(ResetCallback callback);
  virtual void Cancel() = 0;
  virtual std::string GetId() const = 0;
  virtual base::string16 GetDisplayName() const = 0;
  virtual ProtocolVersion SupportedProtocol() const;
  virtual const base::Optional<AuthenticatorSupportedOptions>& Options()
      const = 0;
  virtual base::Optional<FidoTransportProtocol> AuthenticatorTransport()
      const = 0;
  virtual bool IsInPairingMode() const = 0;
  virtual bool IsPaired() const = 0;
#if defined(OS_WIN)
  virtual bool IsWinNativeApiAuthenticator() const = 0;
#endif  // defined(OS_WIN)
  virtual base::WeakPtr<FidoAuthenticator> GetWeakPtr() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FidoAuthenticator);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_AUTHENTICATOR_H_
