// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/register_response_data.h"

#include <utility>

#include "device/u2f/attestation_object.h"
#include "device/u2f/attested_credential_data.h"
#include "device/u2f/authenticator_data.h"
#include "device/u2f/ec_public_key.h"
#include "device/u2f/fido_attestation_statement.h"
#include "device/u2f/u2f_parsing_utils.h"

namespace device {

// static
base::Optional<RegisterResponseData>
RegisterResponseData::CreateFromU2fRegisterResponse(
    std::string relying_party_id,
    base::span<const uint8_t> u2f_data) {
  std::unique_ptr<ECPublicKey> public_key =
      ECPublicKey::ExtractFromU2fRegistrationResponse(u2f_parsing_utils::kEs256,
                                                      u2f_data);

  if (!public_key) {
    return base::nullopt;
  }

  // Construct the attestation data.
  // AAGUID is zeroed out for U2F responses.
  std::vector<uint8_t> aaguid(16u);

  auto attested_credential_data =
      AttestedCredentialData::CreateFromU2fRegisterResponse(
          u2f_data, std::move(aaguid), std::move(public_key));

  if (!attested_credential_data) {
    return base::nullopt;
  }

  // Extract the credential_id for packing into the reponse data.
  std::vector<uint8_t> credential_id =
      attested_credential_data->credential_id();

  // Construct the authenticator data.
  // The counter is zeroed out for Register requests.
  std::vector<uint8_t> counter(4u);
  constexpr uint8_t flags =
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence) |
      static_cast<uint8_t>(AuthenticatorData::Flag::kAttestation);

  AuthenticatorData authenticator_data(std::move(relying_party_id), flags,
                                       std::move(counter),
                                       std::move(attested_credential_data));

  // Construct the attestation statement.
  auto fido_attestation_statement =
      FidoAttestationStatement::CreateFromU2fRegisterResponse(u2f_data);

  if (!fido_attestation_statement) {
    return base::nullopt;
  }

  return RegisterResponseData(std::move(credential_id),
                              std::make_unique<AttestationObject>(
                                  std::move(authenticator_data),
                                  std::move(fido_attestation_statement)));
}

RegisterResponseData::RegisterResponseData() = default;

RegisterResponseData::RegisterResponseData(
    std::vector<uint8_t> credential_id,
    std::unique_ptr<AttestationObject> object)
    : ResponseData(std::move(credential_id)),
      attestation_object_(std::move(object)) {}

RegisterResponseData::RegisterResponseData(RegisterResponseData&& other) =
    default;

RegisterResponseData& RegisterResponseData::operator=(
    RegisterResponseData&& other) = default;

RegisterResponseData::~RegisterResponseData() = default;

std::vector<uint8_t> RegisterResponseData::GetCBOREncodedAttestationObject()
    const {
  return attestation_object_->SerializeToCBOREncodedBytes();
}

}  // namespace device
