// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_WEBAUTHN_API_ADAPTER_H_
#define DEVICE_FIDO_WIN_WEBAUTHN_API_ADAPTER_H_

#include <windows.h>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/threading/thread.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/win/webauthn_api.h"

namespace device {

// WinWebAuthnApiAdapter is the adapter class that creates the data structures
// used in the Windows WebAuthn API. It owns the thread for the WebAuthn
// operation.
//
// Users must check the result of |IsAvailable| on the instance to verify that
// the native library was loaded successfully before invoking any of the other
// methods. All WinWebAuthnApiAdapters use the same WebAuthnApi reference.
//
// TODO(martinkr): Perhaps this should be folded back into
// |WinWebAuthnApiAuthenticator|.
class COMPONENT_EXPORT(DEVICE_FIDO) WinWebAuthnApiAdapter {
 public:
  // ScopedCredentialAttestation is a scoped deleter for a
  // WEB_AUTHN_CREDENTIAL_ATTESTATION pointer.
  //
  // Instances must not outlive their WinWebAuthnApi.
  using ScopedCredentialAttestation =
      std::unique_ptr<WEBAUTHN_CREDENTIAL_ATTESTATION,
                      std::function<void(PWEBAUTHN_CREDENTIAL_ATTESTATION)>>;

  // ScopedAssertion is a scoped deleter for a WEB_AUTHN_ASSERTION pointer.
  //
  // Instances must not outlive their WinWebAuthnApi.
  using ScopedAssertion =
      std::unique_ptr<WEBAUTHN_ASSERTION,
                      std::function<void(PWEBAUTHN_ASSERTION)>>;

  using AuthenticatorMakeCredentialCallback =
      base::OnceCallback<void(HRESULT, ScopedCredentialAttestation)>;
  using AuthenticatorGetAssertionCallback =
      base::OnceCallback<void(HRESULT, ScopedAssertion)>;

  // Returns whether the API is available on this system. If this returns
  // false, this class should not be instantiated and none of its instance
  // methods must be called.
  static bool IsAvailable();

  WinWebAuthnApiAdapter();
  ~WinWebAuthnApiAdapter();

  // See WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable in <webauthn.h>.
  HRESULT IsUserVerifyingPlatformAuthenticatorAvailable(BOOL* available);

  // See WebAuthNAuthenticatorMakeCredential in <webauthn.h>.
  //
  // The following fields in |options| are ignored because they get filled in
  // from the other parameters:
  //  - Extensions
  //  - pCancellationId
  //  - CredentialList / pExcludeCredentialList
  void AuthenticatorMakeCredential(
      HWND h_wnd,
      GUID cancellation_id,
      PublicKeyCredentialRpEntity rp,
      PublicKeyCredentialUserEntity user,
      std::vector<WEBAUTHN_COSE_CREDENTIAL_PARAMETER>
          cose_credential_parameter_values,
      std::string client_data_json,
      std::vector<WEBAUTHN_EXTENSION> extensions,
      base::Optional<std::vector<PublicKeyCredentialDescriptor>> exclude_list,
      WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options,
      AuthenticatorMakeCredentialCallback callback);

  // See WebAuthNAuthenticatorGetAssertion in <webauthn.h>.
  //
  // The following fields in |options| are ignored because they get filled in
  // from the other parameters:
  //  - pwszU2fAppId / pbU2fAppId
  //  - pCancellationId
  //  - CredentialList / pAllowCredentialList
  void AuthenticatorGetAssertion(
      HWND h_wnd,
      GUID cancellation_id,
      base::string16 rp_id,
      base::Optional<base::string16> opt_app_id,
      std::string client_data_json,
      base::Optional<std::vector<PublicKeyCredentialDescriptor>> allow_list,
      WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options,
      AuthenticatorGetAssertionCallback callback);

  // See WebAuthNCancelCurrentOperation in <webauthn.h>.
  HRESULT CancelCurrentOperation(GUID* cancellation_id);

  // See WebAuthNGetErrorName in <webauthn.h>.
  const wchar_t* GetErrorName(HRESULT hr);

 private:
  WinWebAuthnApiAdapter(const WinWebAuthnApiAdapter&) = delete;
  WinWebAuthnApiAdapter& operator=(const WinWebAuthnApiAdapter&) = delete;

  static void AuthenticatorMakeCredentialBlocking(
      WinWebAuthnApi* webauthn_api,
      HWND h_wnd,
      GUID cancellation_id,
      PublicKeyCredentialRpEntity rp,
      PublicKeyCredentialUserEntity user,
      std::vector<WEBAUTHN_COSE_CREDENTIAL_PARAMETER>
          cose_credential_parameter_values,
      std::string client_data_json,
      std::vector<WEBAUTHN_EXTENSION> extensions,
      base::Optional<std::vector<PublicKeyCredentialDescriptor>> exclude_list,
      WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options,
      scoped_refptr<base::SequencedTaskRunner> callback_runner,
      base::OnceCallback<void(std::pair<HRESULT, ScopedCredentialAttestation>)>
          callback);

  static void AuthenticatorGetAssertionBlocking(
      WinWebAuthnApi* webauthn_api,
      HWND h_wnd,
      GUID cancellation_id,
      base::string16 rp_id,
      base::Optional<base::string16> opt_app_id,
      std::string client_data_json,
      base::Optional<std::vector<PublicKeyCredentialDescriptor>> allow_list,
      WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options,
      scoped_refptr<base::SequencedTaskRunner> callback_runner,
      base::OnceCallback<void(std::pair<HRESULT, ScopedAssertion>)> callback);

  void AuthenticatorMakeCredentialDone(
      AuthenticatorMakeCredentialCallback callback,
      std::pair<HRESULT, ScopedCredentialAttestation> result);

  void AuthenticatorGetAssertionDone(
      AuthenticatorGetAssertionCallback callback,
      std::pair<HRESULT, ScopedAssertion> result);

  // The pointee of |webauthn_api_| is assumed to be a singleton that outlives
  // this instance.
  WinWebAuthnApi* webauthn_api_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<WinWebAuthnApiAdapter> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_FIDO_WIN_WEBAUTHN_API_ADAPTER_H_
