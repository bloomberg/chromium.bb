// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/webauthn_api.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/native_library.h"
#include "base/no_destructor.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "device/fido/features.h"

namespace device {

// We do not integrate with older API versions of webauthn.dll because they
// don't support BLE and direct device access to USB and BLE FIDO devices is
// not yet blocked on those platforms.
constexpr uint32_t kMinWinWebAuthnApiVersion = WEBAUTHN_API_VERSION_1;

// WinWebAuthnApiImpl is the default implementation of WinWebAuthnApi, which
// attempts to load webauthn.dll on intialization.
class WinWebAuthnApiImpl : public WinWebAuthnApi {
 public:
  WinWebAuthnApiImpl()
      : is_bound_(false),
        thread_(std::make_unique<base::Thread>("WindowsWebAuthnAPIRequest")) {
    static HMODULE webauthn_dll = LoadLibraryA("webauthn.dll");
    if (!webauthn_dll) {
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
                      webauthn_dll,
                      "WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable");
    BIND_FN_OR_RETURN(authenticator_make_credential_, webauthn_dll,
                      "WebAuthNAuthenticatorMakeCredential");
    BIND_FN_OR_RETURN(authenticator_get_assertion_, webauthn_dll,
                      "WebAuthNAuthenticatorGetAssertion");
    BIND_FN_OR_RETURN(cancel_current_operation_, webauthn_dll,
                      "WebAuthNCancelCurrentOperation");
    BIND_FN_OR_RETURN(get_error_name_, webauthn_dll, "WebAuthNGetErrorName");
    BIND_FN_OR_RETURN(free_credential_attestation_, webauthn_dll,
                      "WebAuthNFreeCredentialAttestation");
    BIND_FN_OR_RETURN(free_assertion_, webauthn_dll, "WebAuthNFreeAssertion");

    is_bound_ = true;

    // Determine the API version of webauthn.dll. There is a version currently
    // shipping with Windows RS5 from before WebAuthNGetApiVersionNumber was
    // added (i.e., before WEBAUTHN_API_VERSION_1). Therefore we allow this
    // function to be missing.
    BIND_FN(get_api_version_number_, webauthn_dll,
            "WebAuthNGetApiVersionNumber");
    api_version_ = get_api_version_number_ ? get_api_version_number_() : 0;
    thread_->Start();
  }

  ~WinWebAuthnApiImpl() override {}

  // WinWebAuthnApi:
  bool IsAvailable() const override {
    return is_bound_ && (api_version_ >= kMinWinWebAuthnApiVersion ||
                         base::FeatureList::IsEnabled(
                             kWebAuthDisableWinApiVersionCheckForTesting));
  }

  HRESULT IsUserVerifyingPlatformAuthenticatorAvailable(BOOL* result) override {
    DCHECK(is_bound_);
    return is_user_verifying_platform_authenticator_available_(result);
  }

  void AuthenticatorMakeCredential(
      HWND h_wnd,
      const WEBAUTHN_RP_ENTITY_INFORMATION* rp_information,
      const WEBAUTHN_USER_ENTITY_INFORMATION* user_information,
      const WEBAUTHN_COSE_CREDENTIAL_PARAMETERS* pub_key_cred_params,
      const WEBAUTHN_CLIENT_DATA* client_data,
      const WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS* options,
      AuthenticatorMakeCredentialCallback callback) override {
    DCHECK(is_bound_);
    base::PostTaskAndReplyWithResult(
        thread_->task_runner().get(), FROM_HERE,
        base::BindOnce(&WinWebAuthnApiImpl::AuthenticatorMakeCredentialBlocking,
                       base::Unretained(this),  // |thread_| is owned by this.
                       h_wnd, rp_information, user_information,
                       pub_key_cred_params, client_data, options),
        base::BindOnce(&WinWebAuthnApiImpl::AuthenticatorMakeCredentialDone,
                       base::Unretained(this),
                       base::SequencedTaskRunnerHandle::Get(),
                       std::move(callback)));
  }

  void AuthenticatorGetAssertion(
      HWND h_wnd,
      const wchar_t* rp_id_utf16,
      const WEBAUTHN_CLIENT_DATA* client_data,
      const WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS* options,
      AuthenticatorGetAssertionCallback callback) override {
    DCHECK(is_bound_);
    base::PostTaskAndReplyWithResult(
        thread_->task_runner().get(), FROM_HERE,
        base::BindOnce(&WinWebAuthnApiImpl::AuthenticatorGetAssertionBlocking,
                       base::Unretained(this),  // |thread_| is owned by this.
                       h_wnd, rp_id_utf16, client_data, options),
        base::BindOnce(&WinWebAuthnApiImpl::AuthenticatorGetAssertionDone,
                       base::Unretained(this),
                       base::SequencedTaskRunnerHandle::Get(),
                       std::move(callback)));
  }

  HRESULT CancelCurrentOperation(GUID* cancellation_id) override {
    return cancel_current_operation_(cancellation_id);
  }

  const wchar_t* GetErrorName(HRESULT hr) override {
    DCHECK(is_bound_);
    return get_error_name_(hr);
  };

 private:
  std::pair<HRESULT, ScopedCredentialAttestation>
  AuthenticatorMakeCredentialBlocking(
      HWND h_wnd,
      const WEBAUTHN_RP_ENTITY_INFORMATION* rp_information,
      const WEBAUTHN_USER_ENTITY_INFORMATION* user_information,
      const WEBAUTHN_COSE_CREDENTIAL_PARAMETERS* pub_key_cred_params,
      const WEBAUTHN_CLIENT_DATA* client_data,
      const WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS* options) {
    WEBAUTHN_CREDENTIAL_ATTESTATION* credential_attestation_ptr = nullptr;
    HRESULT hresult = authenticator_make_credential_(
        h_wnd, rp_information, user_information, pub_key_cred_params,
        client_data, options, &credential_attestation_ptr);
    return std::make_pair(
        hresult, ScopedCredentialAttestation(credential_attestation_ptr,
                                             free_credential_attestation_));
  }

  std::pair<HRESULT, ScopedAssertion> AuthenticatorGetAssertionBlocking(
      HWND h_wnd,
      const wchar_t* rp_id_utf16,
      const WEBAUTHN_CLIENT_DATA* client_data,
      const WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS* options) {
    WEBAUTHN_ASSERTION* assertion_ptr = nullptr;
    HRESULT hresult = authenticator_get_assertion_(
        h_wnd, rp_id_utf16, client_data, options, &assertion_ptr);
    return std::make_pair(hresult,
                          ScopedAssertion(assertion_ptr, free_assertion_));
  }

  void AuthenticatorMakeCredentialDone(
      scoped_refptr<base::SequencedTaskRunner> callback_runner,
      AuthenticatorMakeCredentialCallback callback,
      std::pair<HRESULT, ScopedCredentialAttestation> result) {
    callback_runner->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), result.first,
                                             std::move(result.second)));
  }

  void AuthenticatorGetAssertionDone(
      scoped_refptr<base::SequencedTaskRunner> callback_runner,
      AuthenticatorGetAssertionCallback callback,
      std::pair<HRESULT, ScopedAssertion> result) {
    callback_runner->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), result.first,
                                             std::move(result.second)));
  }

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

  bool is_bound_ = false;
  uint32_t api_version_ = 0;

  std::unique_ptr<base::Thread> thread_;
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

WinWebAuthnApi::~WinWebAuthnApi() = default;

}  // namespace device
