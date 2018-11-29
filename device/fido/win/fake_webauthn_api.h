// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_FAKE_WEBAUTHN_API_H_
#define DEVICE_FIDO_WIN_FAKE_WEBAUTHN_API_H_

#include "base/macros.h"
#include "device/fido/win/webauthn_api.h"

namespace device {

class FakeWinWebAuthnApi : public WinWebAuthnApi {
 public:
  FakeWinWebAuthnApi();
  ~FakeWinWebAuthnApi() override;

  // Inject the return value for WinWebAuthnApi::IsAvailable().
  void set_available(bool available) { is_available_ = available; }
  // Inject the return value for
  // WinWebAuthnApi::IsUserverifyingPlatformAuthenticatorAvailable().
  void set_is_uvpaa(bool is_uvpaa) { is_uvpaa_ = is_uvpaa; }

  // WinWebAuthnApi:
  bool IsAvailable() const override;
  // The following methods all return E_NOTIMPL immediately.
  HRESULT IsUserVerifyingPlatformAuthenticatorAvailable(
      BOOL* available) override;
  void AuthenticatorMakeCredential(
      HWND h_wnd,
      const WEBAUTHN_RP_ENTITY_INFORMATION* rp_information,
      const WEBAUTHN_USER_ENTITY_INFORMATION* user_information,
      const WEBAUTHN_COSE_CREDENTIAL_PARAMETERS* pub_key_cred_params,
      const WEBAUTHN_CLIENT_DATA* client_data,
      const WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS* options,
      AuthenticatorMakeCredentialCallback callback) override;
  void AuthenticatorGetAssertion(
      HWND h_wnd,
      const wchar_t* rp_id_utf16,
      const WEBAUTHN_CLIENT_DATA* client_data,
      const WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS* options,
      AuthenticatorGetAssertionCallback callback) override;
  HRESULT CancelCurrentOperation(GUID* cancellation_id) override;
  // Returns L"not implemented".
  const wchar_t* GetErrorName(HRESULT hr) override;

 private:
  bool is_available_ = true;
  bool is_uvpaa_ = false;
};

// ScopedFakeWinWebAuthnApi overrides the value returned
// by WinWebAuthnApi::GetDefault with itself for the duration of its
// lifetime.
class ScopedFakeWinWebAuthnApi : public FakeWinWebAuthnApi {
 public:
  ScopedFakeWinWebAuthnApi();
  ~ScopedFakeWinWebAuthnApi() override;
};

}  // namespace device

#endif  // DEVICE_FIDO_WIN_FAKE_WEBAUTHN_API_H_
