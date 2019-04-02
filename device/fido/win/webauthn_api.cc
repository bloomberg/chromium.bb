// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/webauthn_api.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"

namespace device {

// We do not integrate with older API versions of webauthn.dll because they
// don't support BLE and direct device access to USB and BLE FIDO devices is
// not yet blocked on those platforms.
constexpr uint32_t kMinWinWebAuthnApiVersion = WEBAUTHN_API_VERSION_1;

class WinWebAuthnApiImpl : public WinWebAuthnApi {
 public:
  WinWebAuthnApiImpl() : WinWebAuthnApi(), is_bound_(false) {
    webauthn_dll_ =
        LoadLibraryExA("webauthn.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!webauthn_dll_) {
      return;
    }

#define BIND_FN(fn_pointer, lib_handle, fn_name)       \
  DCHECK(!fn_pointer);                                 \
  fn_pointer = reinterpret_cast<decltype(fn_pointer)>( \
      GetProcAddress(lib_handle, fn_name));

#define BIND_FN_OR_RETURN(fn_pointer, lib_handle, fn_name) \
  BIND_FN(fn_pointer, lib_handle, fn_name);                \
  if (!fn_pointer) {                                       \
    DLOG(ERROR) << "failed to bind " << fn_name;           \
    return;                                                \
  }

    BIND_FN_OR_RETURN(is_user_verifying_platform_authenticator_available_,
                      webauthn_dll_,
                      "WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable");
    BIND_FN_OR_RETURN(authenticator_make_credential_, webauthn_dll_,
                      "WebAuthNAuthenticatorMakeCredential");
    BIND_FN_OR_RETURN(authenticator_get_assertion_, webauthn_dll_,
                      "WebAuthNAuthenticatorGetAssertion");
    BIND_FN_OR_RETURN(cancel_current_operation_, webauthn_dll_,
                      "WebAuthNCancelCurrentOperation");
    BIND_FN_OR_RETURN(get_error_name_, webauthn_dll_, "WebAuthNGetErrorName");
    BIND_FN_OR_RETURN(free_credential_attestation_, webauthn_dll_,
                      "WebAuthNFreeCredentialAttestation");
    BIND_FN_OR_RETURN(free_assertion_, webauthn_dll_, "WebAuthNFreeAssertion");

    is_bound_ = true;

    // Determine the API version of webauthn.dll. There is a version currently
    // shipping with Windows RS5 from before WebAuthNGetApiVersionNumber was
    // added (i.e., before WEBAUTHN_API_VERSION_1). Therefore we allow this
    // function to be missing.
    BIND_FN(get_api_version_number_, webauthn_dll_,
            "WebAuthNGetApiVersionNumber");
    api_version_ = get_api_version_number_ ? get_api_version_number_() : 0;
  }

  // WinWebAuthnApi:
  bool IsAvailable() const override {
    return is_bound_ && (api_version_ >= kMinWinWebAuthnApiVersion);
  }

  HRESULT IsUserVerifyingPlatformAuthenticatorAvailable(
      BOOL* available) override {
    DCHECK(is_bound_);
    return is_user_verifying_platform_authenticator_available_(available);
  }

  HRESULT AuthenticatorMakeCredential(
      HWND h_wnd,
      PCWEBAUTHN_RP_ENTITY_INFORMATION rp,
      PCWEBAUTHN_USER_ENTITY_INFORMATION user,
      PCWEBAUTHN_COSE_CREDENTIAL_PARAMETERS cose_credential_parameters,
      PCWEBAUTHN_CLIENT_DATA client_data,
      PCWEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options,
      PWEBAUTHN_CREDENTIAL_ATTESTATION* credential_attestation_ptr) override {
    DCHECK(is_bound_);
    return authenticator_make_credential_(
        h_wnd, rp, user, cose_credential_parameters, client_data, options,
        credential_attestation_ptr);
  }

  HRESULT AuthenticatorGetAssertion(
      HWND h_wnd,
      LPCWSTR rp_id,
      PCWEBAUTHN_CLIENT_DATA client_data,
      PCWEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options,
      PWEBAUTHN_ASSERTION* assertion_ptr) override {
    DCHECK(is_bound_);
    return authenticator_get_assertion_(h_wnd, rp_id, client_data, options,
                                        assertion_ptr);
  }

  HRESULT CancelCurrentOperation(GUID* cancellation_id) override {
    DCHECK(is_bound_);
    return cancel_current_operation_(cancellation_id);
  }

  PCWSTR GetErrorName(HRESULT hr) override {
    DCHECK(is_bound_);
    return get_error_name_(hr);
  }

  void FreeCredentialAttestation(
      PWEBAUTHN_CREDENTIAL_ATTESTATION attestation_ptr) override {
    DCHECK(is_bound_);
    return free_credential_attestation_(attestation_ptr);
  }
  void FreeAssertion(PWEBAUTHN_ASSERTION assertion_ptr) override {
    DCHECK(is_bound_);
    return free_assertion_(assertion_ptr);
  }

  ~WinWebAuthnApiImpl() override {}

 private:
  bool is_bound_ = false;
  uint32_t api_version_ = 0;
  HMODULE webauthn_dll_;

  decltype(&WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable)
      is_user_verifying_platform_authenticator_available_ = nullptr;
  decltype(
      &WebAuthNAuthenticatorMakeCredential) authenticator_make_credential_ =
      nullptr;
  decltype(&WebAuthNAuthenticatorGetAssertion) authenticator_get_assertion_ =
      nullptr;
  decltype(&WebAuthNCancelCurrentOperation) cancel_current_operation_ = nullptr;
  decltype(&WebAuthNGetErrorName) get_error_name_ = nullptr;
  decltype(&WebAuthNFreeCredentialAttestation) free_credential_attestation_ =
      nullptr;
  decltype(&WebAuthNFreeAssertion) free_assertion_ = nullptr;

  // This method is not available in all versions of webauthn.dll.
  decltype(&WebAuthNGetApiVersionNumber) get_api_version_number_ = nullptr;
};

static WinWebAuthnApi* kDefaultForTesting = nullptr;

// static
WinWebAuthnApi* WinWebAuthnApi::GetDefault() {
  if (kDefaultForTesting) {
    return kDefaultForTesting;
  }

  static base::NoDestructor<WinWebAuthnApiImpl> api;
  return api.get();
}

// static
void WinWebAuthnApi::SetDefaultForTesting(WinWebAuthnApi* api) {
  DCHECK(!kDefaultForTesting);
  kDefaultForTesting = api;
}

// static
void WinWebAuthnApi::ClearDefaultForTesting() {
  DCHECK(kDefaultForTesting);
  kDefaultForTesting = nullptr;
}

WinWebAuthnApi::WinWebAuthnApi() = default;
WinWebAuthnApi::~WinWebAuthnApi() = default;

}  // namespace device
