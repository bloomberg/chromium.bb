// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_WEBAUTHN_API_H_
#define DEVICE_FIDO_WIN_WEBAUTHN_API_H_

#include <windows.h>
#include <functional>
#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "third_party/microsoft_webauthn/webauthn.h"

namespace device {

// WinWebAuthnApi is a wrapper for the native Windows WebAuthn API.
//
// The default singleton instance can be obtained by calling |GetDefault|.
// Users must check the result of |IsAvailable| on the instance to verify that
// the native library was loaded successfully before invoking any of the other
// methods.
class COMPONENT_EXPORT(DEVICE_FIDO) WinWebAuthnApi {
 public:
  // ScopedCredentialAttestation is a scoped deleter for a
  // WEB_AUTHN_CREDENTIAL_ATTESTATION pointer.
  //
  // Instances must not outlive their WinWebAuthnApi.
  using ScopedCredentialAttestation =
      std::unique_ptr<WEBAUTHN_CREDENTIAL_ATTESTATION,
                      std::function<void(WEBAUTHN_CREDENTIAL_ATTESTATION*)>>;

  // ScopedAssertion is a scoped deleter for a WEB_AUTHN_ASSERTION pointer.
  //
  // Instances must not outlive their WinWebAuthnApi.
  using ScopedAssertion =
      std::unique_ptr<WEBAUTHN_ASSERTION,
                      std::function<void(WEBAUTHN_ASSERTION*)>>;

  using AuthenticatorMakeCredentialCallback =
      base::OnceCallback<void(HRESULT, ScopedCredentialAttestation)>;
  using AuthenticatorGetAssertionCallback =
      base::OnceCallback<void(HRESULT, ScopedAssertion)>;

  // Returns the default implementation of WinWebAuthnApi backed by
  // webauthn.dll. May return nullptr if webauthn.dll cannot be loaded.
  static WinWebAuthnApi* GetDefault();

  virtual ~WinWebAuthnApi();

  // Returns whether the API is available on this system. If this returns
  // false, none of the other methods on this instance may be called.
  virtual bool IsAvailable() const = 0;

  // See WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable in <webauthn.h>.
  virtual HRESULT IsUserVerifyingPlatformAuthenticatorAvailable(
      BOOL* available) = 0;

  // See WebAuthNAuthenticatorMakeCredential in <webauthn.h>.
  //
  // The lifetime of |credential_attestation| must not exceed the lifetime of
  // the WinWebAuthnApi instance.
  virtual void AuthenticatorMakeCredential(
      HWND h_wnd,
      const WEBAUTHN_RP_ENTITY_INFORMATION* rp_information,
      const WEBAUTHN_USER_ENTITY_INFORMATION* user_information,
      const WEBAUTHN_COSE_CREDENTIAL_PARAMETERS* pub_key_cred_params,
      const WEBAUTHN_CLIENT_DATA* client_data,
      const WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS* options,
      AuthenticatorMakeCredentialCallback callback) = 0;

  // See WebAuthNAuthenticatorGetAssertion in <webauthn.h>.
  //
  // The lifetime of |assertion| must not exceed the lifetime of
  // the WinWebAuthnApi instance.
  virtual void AuthenticatorGetAssertion(
      HWND h_wnd,
      const wchar_t* rp_id_utf16,
      const WEBAUTHN_CLIENT_DATA* client_data,
      const WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS* options,
      AuthenticatorGetAssertionCallback callback) = 0;

  // See WebAuthNCancelCurrentOperation in <webauthn.h>.
  virtual HRESULT CancelCurrentOperation(GUID* cancellation_id) = 0;

  // See WebAuthNGetErrorName in <webauthn.h>.
  virtual const wchar_t* GetErrorName(HRESULT hr) = 0;

 private:
  friend class ScopedFakeWinWebAuthnApi;
  static void SetDefaultForTesting(WinWebAuthnApi* api);
  static void ClearDefaultForTesting();
};

}  // namespace device

#endif  // DEVICE_FIDO_WIN_WEBAUTHN_API_H_
