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

  is_pending_ = true;

  auto rp = request.rp();
  auto user = request.user();
  std::string client_data_json = request.client_data_json();
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
  std::vector<WEBAUTHN_EXTENSION> extensions;
  if (request.hmac_secret()) {
    static BOOL kHMACSecretTrue = TRUE;
    extensions.emplace_back(
        WEBAUTHN_EXTENSION{WEBAUTHN_EXTENSIONS_IDENTIFIER_HMAC_SECRET,
                           sizeof(BOOL), static_cast<void*>(&kHMACSecretTrue)});
  }
  auto exclude_list = request.exclude_list();

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
  win_api_->AuthenticatorMakeCredential(
      current_window_, cancellation_id_, std::move(rp), std::move(user),
      std::move(cose_credential_parameter_values), std::move(client_data_json),
      std::move(extensions), std::move(exclude_list),
      WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS{
          WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_3,
          kWinWebAuthnTimeoutMilliseconds,
          WEBAUTHN_CREDENTIALS{
              0, nullptr},  // Ignored because pExcludeCredentialList is set.
          WEBAUTHN_EXTENSIONS{0, nullptr},  // will be set later
          authenticator_attachment, request.resident_key_required(),
          ToWinUserVerificationRequirement(request.user_verification()),
          WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT, 0 /* flags */,
          nullptr,  // pCancellationId -- will be set later
          nullptr,  // pExcludeCredentialList -- will be set later
      },
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
  DCHECK(!is_pending_);
  if (is_pending_)
    return;

  is_pending_ = true;

  base::string16 rp_id16 = base::UTF8ToUTF16(request.rp_id());
  base::Optional<base::string16> opt_app_id16 = base::nullopt;
  if (request.app_id()) {
    opt_app_id16 = base::UTF8ToUTF16(base::StringPiece(
        reinterpret_cast<const char*>(request.app_id()->data()),
        request.app_id()->size()));
  }
  std::string client_data_json = request.client_data_json();
  auto allow_list = request.allow_list();

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

  win_api_->AuthenticatorGetAssertion(
      current_window_, cancellation_id_, std::move(rp_id16),
      std::move(opt_app_id16), std::move(client_data_json),
      std::move(allow_list),
      WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS{
          WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_4,
          kWinWebAuthnTimeoutMilliseconds,
          WEBAUTHN_CREDENTIALS{
              0, nullptr},  // Ignored because pAllowCredentialList is set.
          WEBAUTHN_EXTENSIONS{0, nullptr},  // None supported.
          authenticator_attachment,
          ToWinUserVerificationRequirement(request.user_verification()),
          0,        // flags
          nullptr,  // pwszU2fAppId -- will be set later
          nullptr,  // pbU2fAppId -- will be set later
          nullptr,  // pCancellationId -- will be set later
          nullptr,  // pAllowCredentialList -- will be set later
      },
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

}  // namespace device
