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
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "device/fido/features.h"
#include "device/fido/win/type_conversions.h"

namespace device {

// We do not integrate with older API versions of webauthn.dll because they
// don't support BLE and direct device access to USB and BLE FIDO devices is
// not yet blocked on those platforms.
constexpr uint32_t kMinWinWebAuthnApiVersion = WEBAUTHN_API_VERSION_1;

namespace {
base::string16 OptionalGURLToUTF16(const base::Optional<GURL>& in) {
  return in ? base::UTF8ToUTF16(in->spec()) : base::string16();
}
}  // namespace

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
      GUID cancellation_id,
      PublicKeyCredentialRpEntity rp,
      PublicKeyCredentialUserEntity user,
      std::vector<WEBAUTHN_COSE_CREDENTIAL_PARAMETER>
          cose_credential_parameter_values,
      std::string client_data_json,
      std::vector<WEBAUTHN_EXTENSION> extensions,
      base::Optional<std::vector<PublicKeyCredentialDescriptor>> exclude_list,
      WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options,
      AuthenticatorMakeCredentialCallback callback) override {
    DCHECK(is_bound_);
    base::PostTaskAndReplyWithResult(
        thread_->task_runner().get(), FROM_HERE,
        base::BindOnce(&WinWebAuthnApiImpl::AuthenticatorMakeCredentialBlocking,
                       base::Unretained(this),  // |thread_| is owned by this.
                       h_wnd, cancellation_id, std::move(rp), std::move(user),
                       std::move(cose_credential_parameter_values),
                       std::move(client_data_json), std::move(extensions),
                       std::move(exclude_list), std::move(options)),
        base::BindOnce(&WinWebAuthnApiImpl::AuthenticatorMakeCredentialDone,
                       base::Unretained(this),
                       base::SequencedTaskRunnerHandle::Get(),
                       std::move(callback)));
  }

  void AuthenticatorGetAssertion(
      HWND h_wnd,
      GUID cancellation_id,
      base::string16 rp_id,
      base::Optional<base::string16> opt_app_id,
      std::string client_data_json,
      base::Optional<std::vector<PublicKeyCredentialDescriptor>> allow_list,
      WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options,
      AuthenticatorGetAssertionCallback callback) override {
    DCHECK(is_bound_);
    base::PostTaskAndReplyWithResult(
        thread_->task_runner().get(), FROM_HERE,
        base::BindOnce(&WinWebAuthnApiImpl::AuthenticatorGetAssertionBlocking,
                       base::Unretained(this),  // |thread_| is owned by this.
                       h_wnd, cancellation_id, std::move(rp_id),
                       std::move(opt_app_id), std::move(client_data_json),
                       std::move(allow_list), std::move(options)),
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
      GUID cancellation_id,
      PublicKeyCredentialRpEntity rp,
      PublicKeyCredentialUserEntity user,
      std::vector<WEBAUTHN_COSE_CREDENTIAL_PARAMETER>
          cose_credential_parameter_values,
      std::string client_data_json,
      std::vector<WEBAUTHN_EXTENSION> extensions,
      base::Optional<std::vector<PublicKeyCredentialDescriptor>> exclude_list,
      WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options) {
    base::string16 rp_id = base::UTF8ToUTF16(rp.rp_id());
    base::string16 rp_name = base::UTF8ToUTF16(rp.rp_name().value_or(""));
    base::string16 rp_icon_url = OptionalGURLToUTF16(rp.rp_icon_url());
    base::string16 user_name = base::UTF8ToUTF16(user.user_name().value_or(""));
    base::string16 user_icon_url = OptionalGURLToUTF16(user.user_icon_url());
    WEBAUTHN_RP_ENTITY_INFORMATION rp_info{
        WEBAUTHN_RP_ENTITY_INFORMATION_CURRENT_VERSION, rp_id.c_str(),
        rp_name.c_str(), rp_icon_url.c_str()};

    base::string16 user_display_name =
        base::UTF8ToUTF16(user.user_display_name().value_or(""));
    WEBAUTHN_USER_ENTITY_INFORMATION user_info{
        WEBAUTHN_USER_ENTITY_INFORMATION_CURRENT_VERSION,
        user.user_id().size(),
        const_cast<unsigned char*>(user.user_id().data()),
        user_name.c_str(),
        user_icon_url.c_str(),
        user_display_name.c_str(),  // This appears to be ignored.
    };

    WEBAUTHN_COSE_CREDENTIAL_PARAMETERS cose_credential_parameters{
        cose_credential_parameter_values.size(),
        cose_credential_parameter_values.data()};

    WEBAUTHN_CLIENT_DATA client_data{
        WEBAUTHN_CLIENT_DATA_CURRENT_VERSION, client_data_json.size(),
        const_cast<unsigned char*>(
            reinterpret_cast<const unsigned char*>(client_data_json.data())),
        WEBAUTHN_HASH_ALGORITHM_SHA_256};

    options.Extensions =
        WEBAUTHN_EXTENSIONS{extensions.size(), extensions.data()};
    options.pCancellationId = &cancellation_id;

    auto exclude_list_credentials = ToWinCredentialExVector(exclude_list);
    auto* exclude_list_ptr = exclude_list_credentials.data();
    WEBAUTHN_CREDENTIAL_LIST credential_list{exclude_list_credentials.size(),
                                             &exclude_list_ptr};
    options.pExcludeCredentialList = &credential_list;

    WEBAUTHN_CREDENTIAL_ATTESTATION* credential_attestation_ptr = nullptr;
    HRESULT hresult = authenticator_make_credential_(
        h_wnd, &rp_info, &user_info, &cose_credential_parameters, &client_data,
        &options, &credential_attestation_ptr);
    return std::make_pair(
        hresult, ScopedCredentialAttestation(credential_attestation_ptr,
                                             free_credential_attestation_));
  }

  std::pair<HRESULT, ScopedAssertion> AuthenticatorGetAssertionBlocking(
      HWND h_wnd,
      GUID cancellation_id,
      base::string16 rp_id,
      base::Optional<base::string16> opt_app_id,
      std::string client_data_json,
      base::Optional<std::vector<PublicKeyCredentialDescriptor>> allow_list,
      WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options) {
    if (opt_app_id) {
      options.pwszU2fAppId = opt_app_id->data();
      static BOOL kUseAppIdTrue = TRUE;  // const
      options.pbU2fAppId = &kUseAppIdTrue;
    } else {
      static BOOL kUseAppIdFalse = FALSE;  // const
      options.pbU2fAppId = &kUseAppIdFalse;
    }
    options.pCancellationId = &cancellation_id;

    auto allow_list_credentials = ToWinCredentialExVector(allow_list);
    auto* allow_list_ptr = allow_list_credentials.data();
    WEBAUTHN_CREDENTIAL_LIST credential_list{allow_list_credentials.size(),
                                             &allow_list_ptr};
    options.pAllowCredentialList = &credential_list;

    WEBAUTHN_CLIENT_DATA client_data{
        WEBAUTHN_CLIENT_DATA_CURRENT_VERSION, client_data_json.size(),
        const_cast<unsigned char*>(
            reinterpret_cast<const unsigned char*>(client_data_json.data())),
        WEBAUTHN_HASH_ALGORITHM_SHA_256};

    WEBAUTHN_ASSERTION* assertion_ptr = nullptr;
    HRESULT hresult = authenticator_get_assertion_(
        h_wnd, rp_id.data(), &client_data, &options, &assertion_ptr);
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
