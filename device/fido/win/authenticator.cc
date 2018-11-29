// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/authenticator.h"

#include <Combaseapi.h>
#include <windows.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/win/type_conversions.h"
#include "third_party/microsoft_webauthn/webauthn.h"

namespace device {

// Time out all Windows API requests after 5 minutes. We maintain our own
// timeout and cancel the operation when it expires, so this value simply needs
// to be larger than the largest internal request timeout.
constexpr uint32_t kWinWebAuthnTimeoutMilliseconds = 1000 * 60 * 5;

namespace {
base::string16 OptionalGURLToUTF16(const base::Optional<GURL>& in) {
  return in ? base::UTF8ToUTF16(in->spec()) : base::string16();
}

}  // namespace

// static
const char WinWebAuthnApiAuthenticator::kAuthenticatorId[] =
    "WinWebAuthnApiAuthenticator";

// static
bool WinWebAuthnApiAuthenticator::
    IsUserVerifyingPlatformAuthenticatorAvailable() {
  BOOL result;
  return WinWebAuthnApi::GetDefault()->IsAvailable() &&
         WinWebAuthnApi::GetDefault()
                 ->IsUserVerifyingPlatformAuthenticatorAvailable(&result) ==
             S_OK &&
         result == TRUE;
}

WinWebAuthnApiAuthenticator::WinWebAuthnApiAuthenticator(
    WinWebAuthnApi* win_api,
    HWND current_window)
    : FidoAuthenticator(),
      win_api_(win_api),
      current_window_(current_window),
      weak_factory_(this) {
  CHECK(win_api_->IsAvailable());
  CoCreateGuid(&cancellation_id_);
}

WinWebAuthnApiAuthenticator::~WinWebAuthnApiAuthenticator() {
  // Cancel in order to dismiss any pending API request and UI dialog and shut
  // down |thread_|.
  Cancel();
}

void WinWebAuthnApiAuthenticator::InitializeAuthenticator(
    base::OnceClosure callback) {
  std::move(callback).Run();
}

void WinWebAuthnApiAuthenticator::MakeCredential(
    CtapMakeCredentialRequest request,
    MakeCredentialCallback callback) {
  DCHECK(!thread_);
  if (thread_) {
    return;
  }

  thread_ = std::make_unique<base::Thread>("WindowsWebAuthnAPIRequest");
  thread_->Start();
  // TODO(martinkr): Perhaps pull conversion of the Windows request/response
  // messages out of |thread_| so we can spawn it later and terminate it
  // earlier?

  thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WinWebAuthnApiAuthenticator::MakeCredentialBlocking,
          // Because |thread_| and its task runner are owned by this
          // authenticator instance, binding to Unretained(this) here is
          // fine. If the instance got destroyed before invocation of the
          // task, so would the task. Once the task is running, destruction
          // of the authenticator instance blocks on the thread exiting.
          base::Unretained(this), std::move(request),
          base::BindOnce(
              &WinWebAuthnApiAuthenticator::InvokeMakeCredentialCallback,
              weak_factory_.GetWeakPtr(), std::move(callback)),
          base::SequencedTaskRunnerHandle::Get()));
}

void WinWebAuthnApiAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                               GetAssertionCallback callback) {
  DCHECK(!thread_);
  if (thread_) {
    return;
  }

  thread_ = std::make_unique<base::Thread>("WindowsWebAuthnAPIRequest");
  thread_->Start();
  thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WinWebAuthnApiAuthenticator::GetAssertionBlocking,
          // Because |thread_| and its task runner are owned by this
          // authenticator instance, binding to Unretained(this) here is
          // fine. If the instance got destroyed before invocation of the
          // task, so would the task. Once the task is running, destruction
          // of the authenticator instance blocks on the thread exiting.
          base::Unretained(this), std::move(request),
          base::BindOnce(
              &WinWebAuthnApiAuthenticator::InvokeGetAssertionCallback,
              weak_factory_.GetWeakPtr(), std::move(callback)),
          base::SequencedTaskRunnerHandle::Get()));
}

// Invokes the blocking WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL API call. This
// method is run on |thread_|. Note that the destructor for this class blocks
// on |thread_| shutdown.
void WinWebAuthnApiAuthenticator::MakeCredentialBlocking(
    CtapMakeCredentialRequest request,
    MakeCredentialCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  base::string16 rp_id(base::UTF8ToUTF16(request.rp().rp_id()));
  base::string16 rp_name(
      base::UTF8ToUTF16(request.rp().rp_name().value_or("")));
  base::string16 rp_icon_url(OptionalGURLToUTF16(request.rp().rp_icon_url()));
  WEBAUTHN_RP_ENTITY_INFORMATION rp_entity_information{
      WEBAUTHN_RP_ENTITY_INFORMATION_CURRENT_VERSION, rp_id.c_str(),
      rp_name.c_str(), rp_icon_url.c_str()};

  base::string16 user_name(
      base::UTF8ToUTF16(request.user().user_name().value_or("")));
  base::string16 user_icon_url(
      OptionalGURLToUTF16(request.user().user_icon_url()));
  base::string16 user_display_name(
      base::UTF8ToUTF16(request.user().user_display_name().value_or("")));
  WEBAUTHN_USER_ENTITY_INFORMATION user_entity_information{
      WEBAUTHN_USER_ENTITY_INFORMATION_CURRENT_VERSION,
      request.user().user_id().size(),
      const_cast<unsigned char*>(request.user().user_id().data()),
      user_name.c_str(),
      user_icon_url.c_str(),
      user_display_name.c_str(),  // This appears to be ignored by Windows UI.
  };

  std::vector<WEBAUTHN_COSE_CREDENTIAL_PARAMETER>
      cose_credential_parameter_values;
  for (const PublicKeyCredentialParams::CredentialInfo& credential_info :
       request.public_key_credential_params().public_key_credential_params()) {
    if (credential_info.type != CredentialType::kPublicKey) {
      continue;
    }
    cose_credential_parameter_values.push_back(
        {WEBAUTHN_COSE_CREDENTIAL_PARAMETER_CURRENT_VERSION,
         WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY, credential_info.algorithm});
  }
  WEBAUTHN_COSE_CREDENTIAL_PARAMETERS cose_credential_parameters = {
      cose_credential_parameter_values.size(),
      cose_credential_parameter_values.data()};

  WEBAUTHN_CLIENT_DATA client_data{
      WEBAUTHN_CLIENT_DATA_CURRENT_VERSION, request.client_data_json().size(),
      const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(
          request.client_data_json().data())),
      WEBAUTHN_HASH_ALGORITHM_SHA_256};

  std::vector<WEBAUTHN_EXTENSION> extensions;
  if (request.hmac_secret()) {
    static BOOL kHMACSecretTrue = TRUE;
    extensions.emplace_back(
        WEBAUTHN_EXTENSION{WEBAUTHN_EXTENSIONS_IDENTIFIER_HMAC_SECRET,
                           sizeof(BOOL), static_cast<void*>(&kHMACSecretTrue)});
  }

  std::vector<_WEBAUTHN_CREDENTIAL_EX> exclude_list =
      ToWinCredentialExVector(request.exclude_list());
  auto* exclude_list_ptr = exclude_list.data();
  _WEBAUTHN_CREDENTIAL_LIST exclude_credential_list{exclude_list.size(),
                                                    &exclude_list_ptr};

  uint32_t authenticator_attachment;
  if (request.is_u2f_only()) {
    authenticator_attachment =
        WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM_U2F_V2;
  } else if (request.is_incognito_mode()) {
    // Disable all platform authenticators in incognito mode. We are going to
    // revisit this in crbug/908622.
    authenticator_attachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM;
  } else {
    authenticator_attachment =
        ToWinAuthenticatorAttachment(request.authenticator_attachment());
  }

  WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS make_credential_options{
      WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_3,
      kWinWebAuthnTimeoutMilliseconds,
      WEBAUTHN_CREDENTIALS{
          0, nullptr},  // Ignored because pExcludeCredentialList is set.
      WEBAUTHN_EXTENSIONS{extensions.size(), extensions.data()},
      authenticator_attachment,
      request.resident_key_required(),
      ToWinUserVerificationRequirement(request.user_verification()),
      WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT,
      0 /* flags */,
      &cancellation_id_,
      &exclude_credential_list,
  };

  // |credential_attestation| must not not outlive |win_api_|.
  WinWebAuthnApi::ScopedCredentialAttestation credential_attestation;
  // This call will block.
  HRESULT hresult = win_api_->AuthenticatorMakeCredential(
      current_window_, &rp_entity_information, &user_entity_information,
      &cose_credential_parameters, &client_data, &make_credential_options,
      &credential_attestation);

  if (operation_cancelled_.IsSet()) {
    // Cancel() was called. Ignore the result.
    return;
  }

  const CtapDeviceResponseCode status =
      hresult == S_OK ? CtapDeviceResponseCode::kSuccess
                      : WinErrorNameToCtapDeviceResponseCode(
                            base::string16(win_api_->GetErrorName(hresult)));

  if (status != CtapDeviceResponseCode::kSuccess) {
    callback_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), status, base::nullopt));
    return;
  }

  base::Optional<AuthenticatorMakeCredentialResponse> response =
      credential_attestation
          ? ToAuthenticatorMakeCredentialResponse(*credential_attestation)
          : base::nullopt;
  if (!response) {
    callback_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  CtapDeviceResponseCode::kCtap2ErrInvalidCBOR,
                                  base::nullopt));
    return;
  }

  if (request.hmac_secret()) {
    bool hmac_secret_extension_ok = false;
    base::span<const WEBAUTHN_EXTENSION* const> extensions(
        &credential_attestation->Extensions.pExtensions,
        credential_attestation->Extensions.cExtensions);
    for (const WEBAUTHN_EXTENSION* extension : extensions) {
      if (extension->pwszExtensionIdentifier ==
              WEBAUTHN_EXTENSIONS_IDENTIFIER_HMAC_SECRET &&
          extension->cbExtension == sizeof(BOOL) &&
          *(static_cast<BOOL*>(extension->pvExtension)) == TRUE) {
        hmac_secret_extension_ok = true;
        break;
      }
    }
    if (!hmac_secret_extension_ok) {
      DLOG(ERROR) << "missing hmacSecret extension";
      callback_runner->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback),
                         CtapDeviceResponseCode::kCtap2ErrUnsupportedExtension,
                         base::nullopt));
      return;
    }
  }

  // Post the callback back onto the runner from which this thread originated.
  callback_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), status, std::move(response)));
}

// Invokes the blocking WEBAUTHN_AUTHENTICATOR_GET_ASSERTION API call. This
// method is run on |thread_|. Note that the destructor for this class blocks
// on |thread_| shutdown.
void WinWebAuthnApiAuthenticator::GetAssertionBlocking(
    CtapGetAssertionRequest request,
    GetAssertionCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  static BOOL kUseAppIdTrue = TRUE;  // const

  base::string16 rp_id16 = base::UTF8ToUTF16(request.rp_id());
  base::Optional<base::string16> opt_app_id16 = base::nullopt;
  // TODO(martinkr): alternative_application_parameter() is already hashed,
  // so this doesn't work. We need to make it store the full AppID.
  if (request.alternative_application_parameter()) {
    opt_app_id16 = base::UTF8ToUTF16(base::StringPiece(
        reinterpret_cast<const char*>(
            request.alternative_application_parameter()->data()),
        request.alternative_application_parameter()->size()));
  }

  WEBAUTHN_CLIENT_DATA client_data{
      WEBAUTHN_CLIENT_DATA_CURRENT_VERSION, request.client_data_json().size(),
      const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(
          request.client_data_json().data())),
      WEBAUTHN_HASH_ALGORITHM_SHA_256};

  std::vector<_WEBAUTHN_CREDENTIAL_EX> allow_list =
      ToWinCredentialExVector(request.allow_list());
  auto* allow_list_ptr = allow_list.data();
  _WEBAUTHN_CREDENTIAL_LIST allow_credential_list{allow_list.size(),
                                                  &allow_list_ptr};

  uint32_t authenticator_attachment;
  if (opt_app_id16) {
    authenticator_attachment =
        WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM_U2F_V2;
  } else if (request.is_incognito_mode()) {
    // Disable all platform authenticators in incognito mode. We are going to
    // revisit this in crbug/908622.
    authenticator_attachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM;
  } else {
    authenticator_attachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY;
  }

  WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS get_assertion_options{
      WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_4,
      kWinWebAuthnTimeoutMilliseconds,
      WEBAUTHN_CREDENTIALS{
          0, nullptr},  // Ignored because pAllowCredentialList is set.
      WEBAUTHN_EXTENSIONS{0, nullptr},
      authenticator_attachment,
      ToWinUserVerificationRequirement(request.user_verification()),
      0,                                               // flags
      opt_app_id16 ? opt_app_id16->c_str() : nullptr,  // pwszU2fAppId
      opt_app_id16 ? &kUseAppIdTrue : nullptr,         // pbU2fAppId
      &cancellation_id_,
      &allow_credential_list,
  };

  // |assertion| must not not outlive |win_api_|.
  WinWebAuthnApi::ScopedAssertion assertion;
  // This call will block.
  HRESULT hresult = win_api_->AuthenticatorGetAssertion(
      current_window_, rp_id16.c_str(), &client_data, &get_assertion_options,
      &assertion);

  if (operation_cancelled_.IsSet()) {
    // Cancel() was called. Ignore the result.
    return;
  }

  const CtapDeviceResponseCode status =
      hresult == S_OK ? CtapDeviceResponseCode::kSuccess
                      : WinErrorNameToCtapDeviceResponseCode(
                            base::string16(win_api_->GetErrorName(hresult)));
  base::Optional<AuthenticatorGetAssertionResponse> response =
      assertion ? ToAuthenticatorGetAssertionResponse(*assertion)
                : base::nullopt;

  // Post the callback back onto the runner from which this thread originated.
  callback_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), status, std::move(response)));
}

void WinWebAuthnApiAuthenticator::Cancel() {
  if (!thread_ || operation_cancelled_.IsSet()) {
    return;
  }
  // |thread_| might be blocked waiting on a response from the API. Tell it to
  // throw away the response it receives right after CancelCurrentOperation()
  // unblocks it.
  operation_cancelled_.Set();
  win_api_->CancelCurrentOperation(&cancellation_id_);
  thread_->Stop();
}

std::string WinWebAuthnApiAuthenticator::GetId() const {
  return kAuthenticatorId;
}

base::string16 WinWebAuthnApiAuthenticator::GetDisplayName() const {
  return base::UTF8ToUTF16(GetId());
}

bool WinWebAuthnApiAuthenticator::IsInPairingMode() const {
  return false;
}

bool WinWebAuthnApiAuthenticator::IsPaired() const {
  return false;
}

base::Optional<FidoTransportProtocol>
WinWebAuthnApiAuthenticator::AuthenticatorTransport() const {
  // The Windows API could potentially use any external or
  // platform authenticator.
  return base::nullopt;
}

const base::Optional<AuthenticatorSupportedOptions>&
WinWebAuthnApiAuthenticator::Options() const {
  // The request can potentially be fulfilled by any device that Windows
  // communicates with, so returning AuthenticatorSupportedOptions really
  // doesn't make much sense.
  static const base::Optional<AuthenticatorSupportedOptions> no_options =
      base::nullopt;
  return no_options;
}

base::WeakPtr<FidoAuthenticator> WinWebAuthnApiAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void WinWebAuthnApiAuthenticator::InvokeMakeCredentialCallback(
    MakeCredentialCallback cb,
    CtapDeviceResponseCode status,
    base::Optional<AuthenticatorMakeCredentialResponse> response) {
  // Check for cancellation on the originating sequence before dispatching
  // the callback.
  if (operation_cancelled_.IsSet()) {
    return;
  }
  std::move(cb).Run(status, std::move(response));
}
void WinWebAuthnApiAuthenticator::InvokeGetAssertionCallback(
    GetAssertionCallback cb,
    CtapDeviceResponseCode status,
    base::Optional<AuthenticatorGetAssertionResponse> response) {
  // Check for cancellation on the originating sequence before dispatching
  // the callback.
  if (operation_cancelled_.IsSet()) {
    return;
  }
  std::move(cb).Run(status, std::move(response));
}

}  // namespace device
