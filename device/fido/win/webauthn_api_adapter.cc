// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "device/fido/win/webauthn_api_adapter.h"

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
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "device/fido/win/type_conversions.h"

namespace device {

namespace {
base::string16 OptionalGURLToUTF16(const base::Optional<GURL>& in) {
  return in ? base::UTF8ToUTF16(in->spec()) : base::string16();
}
}  // namespace

WinWebAuthnApiAdapter::WinWebAuthnApiAdapter()
    : webauthn_api_(WinWebAuthnApi::GetDefault()), weak_factory_(this) {
  DCHECK(webauthn_api_->IsAvailable());
}

WinWebAuthnApiAdapter::~WinWebAuthnApiAdapter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
bool WinWebAuthnApiAdapter::IsAvailable() {
  return WinWebAuthnApi::GetDefault()->IsAvailable();
}

HRESULT WinWebAuthnApiAdapter::IsUserVerifyingPlatformAuthenticatorAvailable(
    BOOL* result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return webauthn_api_->IsUserVerifyingPlatformAuthenticatorAvailable(result);
}

void WinWebAuthnApiAdapter::AuthenticatorMakeCredential(
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
    AuthenticatorMakeCredentialCallback callback) {
  DCHECK(webauthn_api_->IsAvailable());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Because |AuthenticatorMakeCredentialBlocking| is posted to some unspecified
  // task runner, |webauthn_api_| must still be alive by the time the task is
  // run. This is okay because |webauthn_api_| is a long-lived singleton.
  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(
          &WinWebAuthnApiAdapter::AuthenticatorMakeCredentialBlocking,
          webauthn_api_, h_wnd, cancellation_id, std::move(rp), std::move(user),
          std::move(cose_credential_parameter_values),
          std::move(client_data_json), std::move(extensions),
          std::move(exclude_list), std::move(options),
          base::SequencedTaskRunnerHandle::Get(),
          base::BindOnce(
              &WinWebAuthnApiAdapter::AuthenticatorMakeCredentialDone,
              weak_factory_.GetWeakPtr(), std::move(callback))));
}

void WinWebAuthnApiAdapter::AuthenticatorGetAssertion(
    HWND h_wnd,
    GUID cancellation_id,
    base::string16 rp_id,
    base::Optional<base::string16> opt_app_id,
    std::string client_data_json,
    base::Optional<std::vector<PublicKeyCredentialDescriptor>> allow_list,
    WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options,
    AuthenticatorGetAssertionCallback callback) {
  DCHECK(webauthn_api_->IsAvailable());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Because |AuthenticatorGetAssertionBlocking| is posted to some unspecified
  // task runner, |webauthn_api_| must still be alive by the time the task is
  // run. This is okay because |webauthn_api_| is a long-lived singleton.
  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(
          &WinWebAuthnApiAdapter::AuthenticatorGetAssertionBlocking,
          webauthn_api_, h_wnd, cancellation_id, std::move(rp_id),
          std::move(opt_app_id), std::move(client_data_json),
          std::move(allow_list), std::move(options),
          base::SequencedTaskRunnerHandle::Get(),
          base::BindOnce(&WinWebAuthnApiAdapter::AuthenticatorGetAssertionDone,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

HRESULT WinWebAuthnApiAdapter::CancelCurrentOperation(GUID* cancellation_id) {
  DCHECK(webauthn_api_->IsAvailable());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return webauthn_api_->CancelCurrentOperation(cancellation_id);
}

const wchar_t* WinWebAuthnApiAdapter::GetErrorName(HRESULT hr) {
  DCHECK(webauthn_api_->IsAvailable());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return webauthn_api_->GetErrorName(hr);
}

// static
void WinWebAuthnApiAdapter::AuthenticatorMakeCredentialBlocking(
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
        callback) {
  DCHECK(webauthn_api->IsAvailable());
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

  std::vector<WEBAUTHN_CREDENTIAL_EX> exclude_list_credentials =
      ToWinCredentialExVector(exclude_list);
  std::vector<WEBAUTHN_CREDENTIAL_EX*> exclude_list_ptrs;
  std::transform(
      exclude_list_credentials.begin(), exclude_list_credentials.end(),
      std::back_inserter(exclude_list_ptrs), [](auto& cred) { return &cred; });
  WEBAUTHN_CREDENTIAL_LIST exclude_credential_list{exclude_list_ptrs.size(),
                                                   exclude_list_ptrs.data()};
  options.pExcludeCredentialList = &exclude_credential_list;

  WEBAUTHN_CREDENTIAL_ATTESTATION* credential_attestation_ptr = nullptr;
  HRESULT hresult = webauthn_api->AuthenticatorMakeCredential(
      h_wnd, &rp_info, &user_info, &cose_credential_parameters, &client_data,
      &options, &credential_attestation_ptr);
  callback_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          std::make_pair(
              hresult,
              ScopedCredentialAttestation(
                  credential_attestation_ptr,
                  [webauthn_api](PWEBAUTHN_CREDENTIAL_ATTESTATION ptr) {
                    webauthn_api->FreeCredentialAttestation(ptr);
                  }))));
}

// static
void WinWebAuthnApiAdapter::AuthenticatorGetAssertionBlocking(
    WinWebAuthnApi* webauthn_api,
    HWND h_wnd,
    GUID cancellation_id,
    base::string16 rp_id,
    base::Optional<base::string16> opt_app_id,
    std::string client_data_json,
    base::Optional<std::vector<PublicKeyCredentialDescriptor>> allow_list,
    WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options,
    scoped_refptr<base::SequencedTaskRunner> callback_runner,
    base::OnceCallback<void(std::pair<HRESULT, ScopedAssertion>)> callback) {
  DCHECK(webauthn_api->IsAvailable());
  if (opt_app_id) {
    options.pwszU2fAppId = opt_app_id->data();
    static BOOL kUseAppIdTrue = TRUE;  // const
    options.pbU2fAppId = &kUseAppIdTrue;
  } else {
    static BOOL kUseAppIdFalse = FALSE;  // const
    options.pbU2fAppId = &kUseAppIdFalse;
  }
  options.pCancellationId = &cancellation_id;

  std::vector<WEBAUTHN_CREDENTIAL_EX> allow_list_credentials =
      ToWinCredentialExVector(allow_list);
  std::vector<WEBAUTHN_CREDENTIAL_EX*> allow_list_ptrs;
  std::transform(allow_list_credentials.begin(), allow_list_credentials.end(),
                 std::back_inserter(allow_list_ptrs),
                 [](auto& cred) { return &cred; });
  WEBAUTHN_CREDENTIAL_LIST allow_credential_list{allow_list_ptrs.size(),
                                                 allow_list_ptrs.data()};
  options.pAllowCredentialList = &allow_credential_list;

  // As of Nov 2018, the WebAuthNAuthenticatorGetAssertion method will fail
  // to challenge credentials via CTAP1 if the allowList is passed in the
  // extended form in WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS (i.e.
  // pAllowCredentialList instead of CredentialList). The legacy
  // CredentialList field works fine, but does not support setting
  // transport restrictions on the credential descriptor.
  //
  // As a workaround, MS tells us to also set the CredentialList parameter
  // with an accurate cCredentials count and some arbitrary pCredentials
  // data.
  auto legacy_credentials = ToWinCredentialVector(allow_list);
  options.CredentialList = WEBAUTHN_CREDENTIALS{legacy_credentials.size(),
                                                legacy_credentials.data()};

  WEBAUTHN_CLIENT_DATA client_data{
      WEBAUTHN_CLIENT_DATA_CURRENT_VERSION, client_data_json.size(),
      const_cast<unsigned char*>(
          reinterpret_cast<const unsigned char*>(client_data_json.data())),
      WEBAUTHN_HASH_ALGORITHM_SHA_256};

  WEBAUTHN_ASSERTION* assertion_ptr = nullptr;
  HRESULT hresult = webauthn_api->AuthenticatorGetAssertion(
      h_wnd, rp_id.data(), &client_data, &options, &assertion_ptr);
  callback_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          std::make_pair(
              hresult, ScopedAssertion(assertion_ptr,
                                       [webauthn_api](PWEBAUTHN_ASSERTION ptr) {
                                         webauthn_api->FreeAssertion(ptr);
                                       }))));
}

void WinWebAuthnApiAdapter::AuthenticatorMakeCredentialDone(
    AuthenticatorMakeCredentialCallback callback,
    std::pair<HRESULT, ScopedCredentialAttestation> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(result.first, std::move(result.second));
}

void WinWebAuthnApiAdapter::AuthenticatorGetAssertionDone(
    AuthenticatorGetAssertionCallback callback,
    std::pair<HRESULT, ScopedAssertion> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(result.first, std::move(result.second));
}

}  // namespace device
