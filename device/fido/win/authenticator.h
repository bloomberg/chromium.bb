// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_AUTHENTICATOR_H_
#define DEVICE_FIDO_WIN_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
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

  static bool IsUserVerifyingPlatformAuthenticatorAvailable();

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
  struct MakeCredentialData {
    MakeCredentialData();
    MakeCredentialData(MakeCredentialData&&);
    MakeCredentialData& operator=(MakeCredentialData&&);
    ~MakeCredentialData();

    base::string16 rp_id;
    base::string16 rp_name;
    base::string16 rp_icon_url;
    WEBAUTHN_RP_ENTITY_INFORMATION rp_entity_information;

    std::vector<uint8_t> user_id;
    base::string16 user_name;
    base::string16 user_icon_url;
    base::string16 user_display_name;
    WEBAUTHN_USER_ENTITY_INFORMATION user_entity_information;

    std::vector<WEBAUTHN_COSE_CREDENTIAL_PARAMETER>
        cose_credential_parameter_values;
    WEBAUTHN_COSE_CREDENTIAL_PARAMETERS cose_credential_parameters;

    std::string request_client_data;
    WEBAUTHN_CLIENT_DATA client_data;
    std::vector<WEBAUTHN_EXTENSION> extensions;
    std::vector<_WEBAUTHN_CREDENTIAL_EX> exclude_list;
    _WEBAUTHN_CREDENTIAL_EX* exclude_list_ptr;
    _WEBAUTHN_CREDENTIAL_LIST exclude_credential_list;

    WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS make_credential_options;

   private:
    DISALLOW_COPY_AND_ASSIGN(MakeCredentialData);
  };

  struct GetAssertionData {
    GetAssertionData();
    GetAssertionData(GetAssertionData&&);
    GetAssertionData& operator=(GetAssertionData&&);
    ~GetAssertionData();

    base::string16 rp_id16;
    std::string request_app_id;
    base::Optional<base::string16> opt_app_id16 = base::nullopt;

    std::string request_client_data;
    WEBAUTHN_CLIENT_DATA client_data;

    std::vector<_WEBAUTHN_CREDENTIAL_EX> allow_list;
    _WEBAUTHN_CREDENTIAL_EX* allow_list_ptr;
    _WEBAUTHN_CREDENTIAL_LIST allow_credential_list;

    WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS get_assertion_options;

   private:
    DISALLOW_COPY_AND_ASSIGN(GetAssertionData);
  };

  // The Windows API is blocking and gets most of its parameters as const
  // pointers. We stash all pointee data in these auxiliary structs while a
  // helper thread performs the blocking API call.
  base::Optional<MakeCredentialData> make_credential_data_;
  base::Optional<GetAssertionData> get_assertion_data_;

  void MakeCredentialDone(
      CtapMakeCredentialRequest request,
      MakeCredentialCallback callback,
      HRESULT result,
      WinWebAuthnApi::ScopedCredentialAttestation credential_attestation);
  void GetAssertionDone(CtapGetAssertionRequest request,
                        GetAssertionCallback callback,
                        HRESULT hresult,
                        WinWebAuthnApi::ScopedAssertion assertion);

  WinWebAuthnApi* win_api_;
  HWND current_window_;

  bool is_pending_ = false;
  bool waiting_for_cancellation_ = false;
  GUID cancellation_id_ = {};
  base::WeakPtrFactory<WinWebAuthnApiAuthenticator> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(WinWebAuthnApiAuthenticator);
};

}  // namespace device

#endif  // DEVICE_FIDO_WIN_AUTHENTICATOR_H_
