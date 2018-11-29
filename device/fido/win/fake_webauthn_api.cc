// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/fake_webauthn_api.h"

#include "base/logging.h"

namespace device {

FakeWinWebAuthnApi::FakeWinWebAuthnApi() = default;
FakeWinWebAuthnApi::~FakeWinWebAuthnApi() = default;

bool FakeWinWebAuthnApi::IsAvailable() const {
  return is_available_;
}

HRESULT FakeWinWebAuthnApi::IsUserVerifyingPlatformAuthenticatorAvailable(
    BOOL* result) {
  DCHECK(is_available_);
  *result = is_uvpaa_;
  return S_OK;
}

HRESULT FakeWinWebAuthnApi::AuthenticatorMakeCredential(
    HWND h_wnd,
    const WEBAUTHN_RP_ENTITY_INFORMATION* rp_information,
    const WEBAUTHN_USER_ENTITY_INFORMATION* user_information,
    const WEBAUTHN_COSE_CREDENTIAL_PARAMETERS* pub_key_cred_params,
    const WEBAUTHN_CLIENT_DATA* client_data,
    const WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS* options,
    ScopedCredentialAttestation* credential_attestation) {
  DCHECK(is_available_);
  return E_NOTIMPL;
}

HRESULT FakeWinWebAuthnApi::AuthenticatorGetAssertion(
    HWND h_wnd,
    const wchar_t* rp_id_utf16,
    const WEBAUTHN_CLIENT_DATA* client_data,
    const WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS* options,
    ScopedAssertion* assertion) {
  DCHECK(is_available_);
  return E_NOTIMPL;
}

HRESULT FakeWinWebAuthnApi::CancelCurrentOperation(GUID* cancellation_id) {
  DCHECK(is_available_);
  return E_NOTIMPL;
}

const wchar_t* FakeWinWebAuthnApi::GetErrorName(HRESULT hr) {
  DCHECK(is_available_);
  return L"not implemented";
};

ScopedFakeWinWebAuthnApi::ScopedFakeWinWebAuthnApi() : FakeWinWebAuthnApi() {
  WinWebAuthnApi::SetDefaultForTesting(this);
}

ScopedFakeWinWebAuthnApi::~ScopedFakeWinWebAuthnApi() {
  WinWebAuthnApi::ClearDefaultForTesting();
}

}  // namespace device
