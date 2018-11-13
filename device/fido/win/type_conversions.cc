// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/type_conversions.h"

#include <vector>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/cbor/reader.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/opaque_attestation_statement.h"

namespace device {

base::Optional<AuthenticatorMakeCredentialResponse>
ToAuthenticatorMakeCredentialResponse(
    const WEBAUTHN_CREDENTIAL_ATTESTATION& credential_attestation) {
  auto authenticator_data = AuthenticatorData::DecodeAuthenticatorData(
      base::span<const uint8_t>(credential_attestation.pbAuthenticatorData,
                                credential_attestation.cbAuthenticatorData));
  if (!authenticator_data) {
    DLOG(ERROR) << "DecodeAuthenticatorData failed: "
                << base::HexEncode(credential_attestation.pbAuthenticatorData,
                                   credential_attestation.cbAuthenticatorData);
    return base::nullopt;
  }
  base::Optional<cbor::Value> cbor_attestation_statement = cbor::Reader::Read(
      base::span<const uint8_t>(credential_attestation.pbAttestation,
                                credential_attestation.cbAttestation));
  if (!cbor_attestation_statement) {
    DLOG(ERROR) << "CBOR decoding attestation statement failed: "
                << base::HexEncode(credential_attestation.pbAttestation,
                                   credential_attestation.cbAttestation);
    return base::nullopt;
  }

  return AuthenticatorMakeCredentialResponse(
      base::nullopt /* transport_used */,
      AttestationObject(
          std::move(*authenticator_data),
          std::make_unique<OpaqueAttestationStatement>(
              base::UTF16ToUTF8(credential_attestation.pwszFormatType),
              std::move(*cbor_attestation_statement))));
}

base::Optional<AuthenticatorGetAssertionResponse>
ToAuthenticatorGetAssertionResponse(const WEBAUTHN_ASSERTION& assertion) {
  auto authenticator_data =
      AuthenticatorData::DecodeAuthenticatorData(base::span<const uint8_t>(
          assertion.pbAuthenticatorData, assertion.cbAuthenticatorData));
  if (!authenticator_data) {
    DLOG(ERROR) << "DecodeAuthenticatorData failed: "
                << base::HexEncode(assertion.pbAuthenticatorData,
                                   assertion.cbAuthenticatorData);
    return base::nullopt;
  }
  AuthenticatorGetAssertionResponse response(
      std::move(*authenticator_data),
      std::vector<uint8_t>(assertion.pbSignature,
                           assertion.pbSignature + assertion.cbSignature));
  if (assertion.Credential.cbId > 0) {
    response.SetCredential(PublicKeyCredentialDescriptor(
        CredentialType::kPublicKey,
        std::vector<uint8_t>(
            assertion.Credential.pbId,
            assertion.Credential.pbId + assertion.Credential.cbId)));
  }
  if (assertion.cbUserId > 0) {
    response.SetUserEntity(PublicKeyCredentialUserEntity(std::vector<uint8_t>(
        assertion.pbUserId, assertion.pbUserId + assertion.cbUserId)));
  }
  return response;
}

uint32_t ToWinUserVerificationRequirement(
    UserVerificationRequirement user_verification_requirement) {
  switch (user_verification_requirement) {
    case UserVerificationRequirement::kRequired:
      return WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
    case UserVerificationRequirement::kPreferred:
      return WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED;
    case UserVerificationRequirement::kDiscouraged:
      return WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED;
  }
  NOTREACHED();
  return WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
}

CtapDeviceResponseCode WinErrorNameToCtapDeviceResponseCode(
    const base::string16& error_name) {
  // TODO(crbug/896522): Another mismatch of our authenticator models. Windows
  // returns WebAuthn authenticator model status, whereas FidoAuthenticator
  // wants to pass on CTAP-level response codes. Do a best effort at mapping
  // them back down for now.
  //
  // See WebAuthNGetErrorName in <webauthn.h> for these string values.
  static base::flat_map<base::string16, CtapDeviceResponseCode>
      kResponseCodeMap({
          {L"Success", CtapDeviceResponseCode::kSuccess},
          // This should be something else for GetAssertion but that currently
          // doesn't make a difference.
          {L"InvalidStateError",
           CtapDeviceResponseCode::kCtap2ErrCredentialExcluded},
          {L"ConstraintError",
           CtapDeviceResponseCode::kCtap2ErrUnsupportedOption},
          {L"NotSupportedError",
           CtapDeviceResponseCode::kCtap2ErrUnsupportedAlgorithms},
          {L"NotAllowedError",
           CtapDeviceResponseCode::kCtap2ErrOperationDenied},
          {L"UnknownError", CtapDeviceResponseCode::kCtap2ErrOther},
      });
  return base::ContainsKey(kResponseCodeMap, error_name)
             ? kResponseCodeMap[error_name]
             : CtapDeviceResponseCode::kCtap2ErrOther;
}

}  // namespace device
