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
  Cancel();
}

void WinWebAuthnApiAuthenticator::InitializeAuthenticator(
    base::OnceClosure callback) {
  std::move(callback).Run();
}

void WinWebAuthnApiAuthenticator::MakeCredential(
    CtapMakeCredentialRequest request,
    MakeCredentialCallback callback) {
  DCHECK(!is_pending_);
  if (is_pending_)
    return;

  DCHECK(!make_credential_data_);
  make_credential_data_ = MakeCredentialData{};
  make_credential_data_->rp_id = base::UTF8ToUTF16(request.rp().rp_id());
  make_credential_data_->rp_name =
      base::UTF8ToUTF16(request.rp().rp_name().value_or(""));
  make_credential_data_->rp_icon_url =
      OptionalGURLToUTF16(request.rp().rp_icon_url());
  make_credential_data_->rp_entity_information = {
      WEBAUTHN_RP_ENTITY_INFORMATION_CURRENT_VERSION,
      make_credential_data_->rp_id.c_str(),
      make_credential_data_->rp_name.c_str(),
      make_credential_data_->rp_icon_url.c_str()};

  make_credential_data_->user_id = request.user().user_id();
  make_credential_data_->user_name =
      base::UTF8ToUTF16(request.user().user_name().value_or(""));
  make_credential_data_->user_icon_url =
      OptionalGURLToUTF16(request.user().user_icon_url());
  make_credential_data_->user_display_name =
      base::UTF8ToUTF16(request.user().user_display_name().value_or(""));
  make_credential_data_->user_entity_information = {
      WEBAUTHN_USER_ENTITY_INFORMATION_CURRENT_VERSION,
      make_credential_data_->user_id.size(),
      const_cast<unsigned char*>(make_credential_data_->user_id.data()),
      make_credential_data_->user_name.c_str(),
      make_credential_data_->user_icon_url.c_str(),
      make_credential_data_->user_display_name
          .c_str(),  // This appears to be ignored by Windows UI.
  };

  for (const PublicKeyCredentialParams::CredentialInfo& credential_info :
       request.public_key_credential_params().public_key_credential_params()) {
    if (credential_info.type != CredentialType::kPublicKey) {
      continue;
    }
    make_credential_data_->cose_credential_parameter_values.push_back(
        {WEBAUTHN_COSE_CREDENTIAL_PARAMETER_CURRENT_VERSION,
         WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY, credential_info.algorithm});
  }
  make_credential_data_->cose_credential_parameters = {
      make_credential_data_->cose_credential_parameter_values.size(),
      make_credential_data_->cose_credential_parameter_values.data()};

  make_credential_data_->request_client_data = request.client_data_json();
  make_credential_data_->client_data = {
      WEBAUTHN_CLIENT_DATA_CURRENT_VERSION,
      make_credential_data_->request_client_data.size(),
      const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(
          make_credential_data_->request_client_data.data())),
      WEBAUTHN_HASH_ALGORITHM_SHA_256};

  if (request.hmac_secret()) {
    static BOOL kHMACSecretTrue = TRUE;
    make_credential_data_->extensions.emplace_back(
        WEBAUTHN_EXTENSION{WEBAUTHN_EXTENSIONS_IDENTIFIER_HMAC_SECRET,
                           sizeof(BOOL), static_cast<void*>(&kHMACSecretTrue)});
  }

  make_credential_data_->exclude_list =
      ToWinCredentialExVector(request.exclude_list());
  make_credential_data_->exclude_list_ptr =
      make_credential_data_->exclude_list.data();
  make_credential_data_->exclude_credential_list = {
      make_credential_data_->exclude_list.size(),
      &make_credential_data_->exclude_list_ptr};

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

  make_credential_data_->make_credential_options = {
      WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_3,
      kWinWebAuthnTimeoutMilliseconds,
      WEBAUTHN_CREDENTIALS{
          0, nullptr},  // Ignored because pExcludeCredentialList is set.
      WEBAUTHN_EXTENSIONS{make_credential_data_->extensions.size(),
                          make_credential_data_->extensions.data()},
      authenticator_attachment,
      request.resident_key_required(),
      ToWinUserVerificationRequirement(request.user_verification()),
      WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT,
      0 /* flags */,
      &cancellation_id_,
      &make_credential_data_->exclude_credential_list,
  };

  is_pending_ = true;
  // TODO(martinkr): This might be unsafe because we don't know whether the
  // Windows API call might touch any of the pointers passed here after the
  // destructor's call to CancelCurrentOperation returns and this object gets
  // deleted. Fixing this requires transferring ownership of the pointees into
  // WinWebAuthnAPI.
  win_api_->AuthenticatorMakeCredential(
      current_window_, &make_credential_data_->rp_entity_information,
      &make_credential_data_->user_entity_information,
      &make_credential_data_->cose_credential_parameters,
      &make_credential_data_->client_data,
      &make_credential_data_->make_credential_options,
      base::BindOnce(&WinWebAuthnApiAuthenticator::MakeCredentialDone,
                     weak_factory_.GetWeakPtr(), std::move(request),
                     std::move(callback)));
}

void WinWebAuthnApiAuthenticator::MakeCredentialDone(
    CtapMakeCredentialRequest request,
    MakeCredentialCallback callback,
    HRESULT hresult,
    WinWebAuthnApi::ScopedCredentialAttestation credential_attestation) {
  DCHECK(is_pending_);
  is_pending_ = false;
  const CtapDeviceResponseCode status =
      hresult == S_OK ? CtapDeviceResponseCode::kSuccess
                      : WinErrorNameToCtapDeviceResponseCode(
                            base::string16(win_api_->GetErrorName(hresult)));
  if (waiting_for_cancellation_) {
    // Don't bother invoking the reply callback if the caller has already
    // cancelled the operation.
    waiting_for_cancellation_ = false;
    return;
  }

  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(status, base::nullopt);
    return;
  }

  base::Optional<AuthenticatorMakeCredentialResponse> response =
      credential_attestation
          ? ToAuthenticatorMakeCredentialResponse(*credential_attestation)
          : base::nullopt;
  if (!response) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrInvalidCBOR,
                            base::nullopt);
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
      std::move(callback).Run(
          CtapDeviceResponseCode::kCtap2ErrUnsupportedExtension, base::nullopt);
      return;
    }
  }

  std::move(callback).Run(status, std::move(response));
}

void WinWebAuthnApiAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                               GetAssertionCallback callback) {
  static BOOL kUseAppIdTrue = TRUE;  // const
  DCHECK(!is_pending_);
  if (is_pending_)
    return;

  DCHECK(!get_assertion_data_);
  get_assertion_data_ = GetAssertionData{};
  get_assertion_data_->rp_id16 = base::UTF8ToUTF16(request.rp_id());
  base::Optional<base::string16> opt_app_id16 = base::nullopt;
  // TODO(martinkr): alternative_application_parameter() is already hashed,
  // so this doesn't work. We need to make it store the full AppID.
  if (request.alternative_application_parameter()) {
    get_assertion_data_->opt_app_id16 = base::UTF8ToUTF16(base::StringPiece(
        reinterpret_cast<const char*>(
            request.alternative_application_parameter()->data()),
        request.alternative_application_parameter()->size()));
  }

  get_assertion_data_->request_client_data = request.client_data_json();
  get_assertion_data_->client_data = {
      WEBAUTHN_CLIENT_DATA_CURRENT_VERSION,
      get_assertion_data_->request_client_data.size(),
      const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(
          get_assertion_data_->request_client_data.data())),
      WEBAUTHN_HASH_ALGORITHM_SHA_256};

  get_assertion_data_->allow_list =
      ToWinCredentialExVector(request.allow_list());
  get_assertion_data_->allow_list_ptr = get_assertion_data_->allow_list.data();
  get_assertion_data_->allow_credential_list = {
      get_assertion_data_->allow_list.size(),
      &get_assertion_data_->allow_list_ptr};

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

  get_assertion_data_->get_assertion_options = {
      WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_4,
      kWinWebAuthnTimeoutMilliseconds,
      WEBAUTHN_CREDENTIALS{
          0, nullptr},  // Ignored because pAllowCredentialList is set.
      WEBAUTHN_EXTENSIONS{0, nullptr},
      authenticator_attachment,
      ToWinUserVerificationRequirement(request.user_verification()),
      0,  // flags
      get_assertion_data_->opt_app_id16
          ? get_assertion_data_->opt_app_id16->c_str()
          : nullptr,  // pwszU2fAppId
      get_assertion_data_->opt_app_id16 ? &kUseAppIdTrue
                                        : nullptr,  // pbU2fAppId
      &cancellation_id_,
      &get_assertion_data_->allow_credential_list,
  };

  is_pending_ = true;
  // TODO(martinkr): This might be unsafe because we don't know whether the
  // Windows API call might touch any of the pointers passed here after the
  // destructor's call to CancelCurrentOperation returns and this object gets
  // deleted. Fixing this requires transferring ownership of the pointees into
  // WinWebAuthnAPI.
  win_api_->AuthenticatorGetAssertion(
      current_window_, get_assertion_data_->rp_id16.c_str(),
      &get_assertion_data_->client_data,
      &get_assertion_data_->get_assertion_options,
      base::BindOnce(&WinWebAuthnApiAuthenticator::GetAssertionDone,
                     weak_factory_.GetWeakPtr(), std::move(request),
                     std::move(callback)));
}

void WinWebAuthnApiAuthenticator::GetAssertionDone(
    CtapGetAssertionRequest request,
    GetAssertionCallback callback,
    HRESULT hresult,
    WinWebAuthnApi::ScopedAssertion assertion) {
  DCHECK(is_pending_);
  is_pending_ = false;
  const CtapDeviceResponseCode status =
      hresult == S_OK ? CtapDeviceResponseCode::kSuccess
                      : WinErrorNameToCtapDeviceResponseCode(
                            base::string16(win_api_->GetErrorName(hresult)));
  if (waiting_for_cancellation_) {
    // Don't bother invoking the reply callback if the caller has already
    // cancelled the operation.
    waiting_for_cancellation_ = false;
    return;
  }

  base::Optional<AuthenticatorGetAssertionResponse> response =
      (hresult == S_OK && assertion)
          ? ToAuthenticatorGetAssertionResponse(*assertion)
          : base::nullopt;
  std::move(callback).Run(status, std::move(response));
}

void WinWebAuthnApiAuthenticator::Cancel() {
  if (!is_pending_ || waiting_for_cancellation_)
    return;

  waiting_for_cancellation_ = true;
  // This returns immediately.
  win_api_->CancelCurrentOperation(&cancellation_id_);
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

WinWebAuthnApiAuthenticator::MakeCredentialData::MakeCredentialData() = default;
WinWebAuthnApiAuthenticator::MakeCredentialData::MakeCredentialData(
    MakeCredentialData&&) = default;
WinWebAuthnApiAuthenticator::MakeCredentialData&
WinWebAuthnApiAuthenticator::MakeCredentialData::operator=(
    MakeCredentialData&&) = default;
WinWebAuthnApiAuthenticator::MakeCredentialData::~MakeCredentialData() =
    default;

WinWebAuthnApiAuthenticator::GetAssertionData::GetAssertionData() = default;
WinWebAuthnApiAuthenticator::GetAssertionData::GetAssertionData(
    GetAssertionData&&) = default;
WinWebAuthnApiAuthenticator::GetAssertionData&
WinWebAuthnApiAuthenticator::GetAssertionData::operator=(GetAssertionData&&) =
    default;
WinWebAuthnApiAuthenticator::GetAssertionData::~GetAssertionData() = default;

}  // namespace device
