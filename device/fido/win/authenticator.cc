// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/authenticator.h"

#include <Combaseapi.h>

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
#include "device/fido/win/webauthn.h"

namespace device {
namespace fido {

// Time out all Windows API requests after 5 minutes. We maintain our own
// timeout and cancel the operation when it expires, so this value simply needs
// to be larger than the largest internal request timeout.
constexpr uint32_t kWinWebAuthnTimeoutMilliseconds = 1000 * 60 * 5;

namespace {
base::string16 OptionalGURLToUTF16(const base::Optional<GURL>& in) {
  return in ? base::UTF8ToUTF16(in->spec()) : base::string16();
}

AuthenticatorSupportedOptions WinNativeCrossPlatformAuthenticatorOptions() {
  AuthenticatorSupportedOptions options;
  // Both MakeCredential and GetAssertion force CROSS_PLATFORM (in CTAP- and in
  // U2F mode).
  options.SetIsPlatformDevice(false);
  // We don't know whether any of the connected USB devices support certain
  // features such as resident keys, client PINs or UV/UP; but they might, and
  // the platform supports them, so we set the "maximum" feature set here.
  options.SetSupportsResidentKey(true);
  options.SetClientPinAvailability(
      AuthenticatorSupportedOptions::ClientPinAvailability::
          kSupportedButPinNotSet);
  options.SetUserVerificationAvailability(
      AuthenticatorSupportedOptions::UserVerificationAvailability::
          kSupportedAndConfigured);
  options.SetUserPresenceRequired(true);
  return options;
}

}  // namespace

WinNativeCrossPlatformAuthenticator::WinNativeCrossPlatformAuthenticator(
    WinWebAuthnApi* win_api,
    HWND current_window)
    : FidoAuthenticator(),
      win_api_(win_api),
      current_window_(current_window),
      weak_factory_(this) {
  CHECK(win_api_->IsAvailable());
  CoCreateGuid(&cancellation_id_);
}

WinNativeCrossPlatformAuthenticator::~WinNativeCrossPlatformAuthenticator() {
  // Cancel in order to dismiss any pending API request and UI dialog and shut
  // down |thread_|.
  Cancel();
}

void WinNativeCrossPlatformAuthenticator::InitializeAuthenticator(
    base::OnceClosure callback) {
  std::move(callback).Run();
}

void WinNativeCrossPlatformAuthenticator::MakeCredential(
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
          &WinNativeCrossPlatformAuthenticator::MakeCredentialBlocking,
          // Because |thread_| and its task runner are owned by this
          // authenticator instance, binding to Unretained(this) here is
          // fine. If the instance got destroyed before invocation of the
          // task, so would the task. Once the task is running, destruction
          // of the authenticator instance blocks on the thread exiting.
          base::Unretained(this), std::move(request),
          base::BindOnce(&WinNativeCrossPlatformAuthenticator::
                             InvokeMakeCredentialCallback,
                         weak_factory_.GetWeakPtr(), std::move(callback)),
          base::SequencedTaskRunnerHandle::Get()));
}

void WinNativeCrossPlatformAuthenticator::GetAssertion(
    CtapGetAssertionRequest request,
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
          &WinNativeCrossPlatformAuthenticator::GetAssertionBlocking,
          // Because |thread_| and its task runner are owned by this
          // authenticator instance, binding to Unretained(this) here is
          // fine. If the instance got destroyed before invocation of the
          // task, so would the task. Once the task is running, destruction
          // of the authenticator instance blocks on the thread exiting.
          base::Unretained(this), std::move(request),
          base::BindOnce(
              &WinNativeCrossPlatformAuthenticator::InvokeGetAssertionCallback,
              weak_factory_.GetWeakPtr(), std::move(callback)),
          base::SequencedTaskRunnerHandle::Get()));
}

// Invokes the blocking WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL API call. This
// method is run on |thread_|. Note that the destructor for this class blocks
// on |thread_| shutdown.
void WinNativeCrossPlatformAuthenticator::MakeCredentialBlocking(
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
      WEBAUTHN_HASH_ALGORITHM_SHA256};

  std::vector<WEBAUTHN_CREDENTIAL> exclude_list;
  if (request.exclude_list()) {
    for (const PublicKeyCredentialDescriptor& desc : *request.exclude_list()) {
      exclude_list.push_back(WEBAUTHN_CREDENTIAL{
          WEBAUTHN_CREDENTIAL_CURRENT_VERSION, desc.id().size(),
          const_cast<unsigned char*>(desc.id().data())});
    }
  }

  // TODO(crbug/896522): The Windows API uses the W3C WebAuthn Authenticator
  // Model abstraction whereas FidoAuthenticator operates at the CTAP2 spec
  // level (e.g. CtapMakeCredentialRequest). As a result we only have the
  // boolean "uv" rather than the enum value from the original request.
  const uint32_t user_verification_requirement =
      request.user_verification_required()
          ? WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED
          : WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED;

  std::vector<WEBAUTHN_EXTENSION> extensions;
  if (request.hmac_secret()) {
    static BOOL kHMACSecretTrue = TRUE;
    extensions.emplace_back(
        WEBAUTHN_EXTENSION{WEBAUTHN_EXTENSIONS_IDENTIFIER_HMAC_SECRET,
                           sizeof(BOOL), static_cast<void*>(&kHMACSecretTrue)});
  }

  WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS make_credential_options{
      WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_CURRENT_VERSION,
      kWinWebAuthnTimeoutMilliseconds,
      WEBAUTHN_CREDENTIALS{exclude_list.size(), exclude_list.data()},
      WEBAUTHN_EXTENSIONS{extensions.size(), extensions.data()},
      // Forcibly set authenticator attachment to cross-platform in order to
      // avoid triggering the platform authenticator option, which is
      // generally displayed first in the Windows UI.
      use_u2f_only_ ? WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM_U2F_V2
                    : WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM,
      request.resident_key_supported(), user_verification_requirement,
      WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT, 0 /* flags */,
      &cancellation_id_};

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
void WinNativeCrossPlatformAuthenticator::GetAssertionBlocking(
    CtapGetAssertionRequest request,
    GetAssertionCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  std::vector<WEBAUTHN_CREDENTIAL> allow_list;
  if (request.allow_list()) {
    for (const PublicKeyCredentialDescriptor& desc : *request.allow_list()) {
      allow_list.push_back(WEBAUTHN_CREDENTIAL{
          WEBAUTHN_CREDENTIAL_CURRENT_VERSION, desc.id().size(),
          const_cast<unsigned char*>(desc.id().data()),
          WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY});
    }
  }

  base::string16 rp_id16 = base::UTF8ToUTF16(request.rp_id());

  WEBAUTHN_CLIENT_DATA client_data{
      WEBAUTHN_CLIENT_DATA_CURRENT_VERSION, request.client_data_json().size(),
      const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(
          request.client_data_json().data())),
      WEBAUTHN_HASH_ALGORITHM_SHA256};

  static BOOL kUseAppIdTrue = TRUE;
  WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS get_assertion_options{
      WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_CURRENT_VERSION,
      kWinWebAuthnTimeoutMilliseconds,
      WEBAUTHN_CREDENTIALS{allow_list.size(), allow_list.data()},
      WEBAUTHN_EXTENSIONS{0, nullptr},
      use_u2f_only_ ? WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM_U2F_V2
                    : WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM,
      ToWinUserVerificationRequirement(request.user_verification()),
      0,                                          // flags
      use_u2f_only_ ? rp_id16.c_str() : nullptr,  // pwszU2fAppId
      use_u2f_only_ ? &kUseAppIdTrue : nullptr,   // pbU2fAppId
      &cancellation_id_,
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

void WinNativeCrossPlatformAuthenticator::Cancel() {
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

std::string WinNativeCrossPlatformAuthenticator::GetId() const {
  return "WinNativeCrossPlatformAuthenticator";
}

base::string16 WinNativeCrossPlatformAuthenticator::GetDisplayName() const {
  return L"WinNativeCrossPlatformAuthenticator";
}

bool WinNativeCrossPlatformAuthenticator::IsInPairingMode() const {
  return false;
}

FidoTransportProtocol
WinNativeCrossPlatformAuthenticator::AuthenticatorTransport() const {
  // TODO(martinkr): Note that this may not necessarily be true if not in
  // U2F-only mode, in which case the authenticator might be NFC, too.
  return FidoTransportProtocol::kUsbHumanInterfaceDevice;
}

const AuthenticatorSupportedOptions&
WinNativeCrossPlatformAuthenticator::Options() const {
  static const AuthenticatorSupportedOptions options =
      WinNativeCrossPlatformAuthenticatorOptions();
  return options;
}

base::WeakPtr<FidoAuthenticator>
WinNativeCrossPlatformAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void WinNativeCrossPlatformAuthenticator::InvokeMakeCredentialCallback(
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
void WinNativeCrossPlatformAuthenticator::InvokeGetAssertionCallback(
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

}  // namespace fido
}  // namespace device
