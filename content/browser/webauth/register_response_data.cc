// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/register_response_data.h"

#include <utility>

#include "content/browser/webauth/attested_credential_data.h"
#include "content/browser/webauth/authenticator_utils.h"
#include "content/browser/webauth/ec_public_key.h"
#include "content/browser/webauth/fido_attestation_statement.h"

namespace content {

// static
std::unique_ptr<RegisterResponseData>
RegisterResponseData::CreateFromU2fRegisterResponse(
    std::unique_ptr<CollectedClientData> client_data,
    std::vector<uint8_t> u2f_data) {
  std::unique_ptr<ECPublicKey> public_key =
      ECPublicKey::ExtractFromU2fRegistrationResponse(
          authenticator_utils::kEs256, u2f_data);

  // Construct the attestation data.
  // AAGUID is zeroed out for U2F responses.
  std::vector<uint8_t> aaguid(16u, 0u);
  std::unique_ptr<AttestedCredentialData> attested_data =
      AttestedCredentialData::CreateFromU2fRegisterResponse(
          u2f_data, std::move(aaguid), std::move(public_key));

  // Extract the credential_id for packing into the reponse data.
  std::vector<uint8_t> credential_id = attested_data->credential_id();

  // Construct the authenticator data.
  // The counter is zeroed out for Register requests.
  std::vector<uint8_t> counter(4u, 0u);
  AuthenticatorData::Flags flags =
      static_cast<AuthenticatorData::Flags>(
          AuthenticatorData::Flag::TEST_OF_USER_PRESENCE) |
      static_cast<AuthenticatorData::Flags>(
          AuthenticatorData::Flag::ATTESTATION);

  std::unique_ptr<AuthenticatorData> authenticator_data =
      AuthenticatorData::Create(client_data->SerializeToJson(), flags,
                                std::move(counter), std::move(attested_data));

  // Construct the attestation statement.
  std::unique_ptr<FidoAttestationStatement> fido_attestation_statement =
      FidoAttestationStatement::CreateFromU2fRegisterResponse(u2f_data);

  // Construct the attestation object.
  auto attestation_object = std::make_unique<AttestationObject>(
      std::move(authenticator_data), std::move(fido_attestation_statement));

  return std::make_unique<RegisterResponseData>(std::move(client_data),
                                                std::move(credential_id),
                                                std::move(attestation_object));
}

RegisterResponseData::RegisterResponseData(
    std::unique_ptr<CollectedClientData> client_data,
    std::vector<uint8_t> credential_id,
    std::unique_ptr<AttestationObject> object)
    : client_data_(std::move(client_data)),
      raw_id_(std::move(credential_id)),
      attestation_object_(std::move(object)) {}

std::vector<uint8_t> RegisterResponseData::GetClientDataJSONBytes() {
  std::string client_data_json = client_data_->SerializeToJson();
  return std::vector<uint8_t>(client_data_json.begin(), client_data_json.end());
}

std::vector<uint8_t> RegisterResponseData::GetCBOREncodedAttestationObject() {
  return attestation_object_->SerializeToCBOREncodedBytes();
}

std::string RegisterResponseData::GetId() {
  std::string id;
  base::Base64UrlEncode(
      base::StringPiece(reinterpret_cast<const char*>(raw_id_.data()),
                        raw_id_.size()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &id);
  return id;
}

RegisterResponseData::~RegisterResponseData() {}

}  // namespace content
