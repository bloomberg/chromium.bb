// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/credential_management.h"

#include "base/logging.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/pin.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace device {

namespace {
std::array<uint8_t, 16> MakePINAuth(base::span<const uint8_t> pin_token,
                                    base::span<const uint8_t> pin_auth_bytes) {
  std::array<uint8_t, SHA256_DIGEST_LENGTH> hmac;
  unsigned hmac_len;
  CHECK(HMAC(EVP_sha256(), pin_token.data(), pin_token.size(),
             pin_auth_bytes.data(), pin_auth_bytes.size(), hmac.data(),
             &hmac_len));
  DCHECK_EQ(hmac.size(), static_cast<size_t>(hmac_len));
  std::array<uint8_t, 16> pin_auth;
  std::copy(hmac.begin(), hmac.begin() + 16, pin_auth.begin());
  return pin_auth;
}

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
MakeCredentialManagementCommand(
    CredentialManagementSubCommand subcommand,
    base::Optional<std::array<uint8_t, 16>> opt_pin_auth) {
  cbor::Value::MapValue request_map;
  request_map.emplace(
      static_cast<int>(CredentialManagementRequestKey::kSubCommand),
      static_cast<int>(subcommand));
  request_map.emplace(
      static_cast<int>(CredentialManagementRequestKey::kPinProtocol),
      static_cast<int>(pin::kProtocolVersion));

  if (opt_pin_auth) {
    request_map.emplace(
        static_cast<int>(CredentialManagementRequestKey::kPinAuth),
        *opt_pin_auth);
  }

  // TODO: allow setting kSubCommandParams.

  return {CtapRequestCommand::kAuthenticatorCredentialManagement,
          cbor::Value(std::move(request_map))};
}
}  // namespace

CredentialsMetadataRequest::CredentialsMetadataRequest(
    std::vector<uint8_t> pin_token_)
    : pin_token(std::move(pin_token_)) {}
CredentialsMetadataRequest::CredentialsMetadataRequest(
    CredentialsMetadataRequest&&) = default;
CredentialsMetadataRequest& CredentialsMetadataRequest::operator=(
    CredentialsMetadataRequest&&) = default;
CredentialsMetadataRequest::~CredentialsMetadataRequest() = default;

// static
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
CredentialsMetadataRequest::EncodeAsCBOR(
    const CredentialsMetadataRequest& request) {
  if (request.pin_token.empty()) {
    return MakeCredentialManagementCommand(
        CredentialManagementSubCommand::kGetCredsMetadata, base::nullopt);
  }

  constexpr std::array<uint8_t, 1> pinauth_bytes = {
      static_cast<uint8_t>(CredentialManagementSubCommand::kGetCredsMetadata)};
  return MakeCredentialManagementCommand(
      CredentialManagementSubCommand::kGetCredsMetadata,
      MakePINAuth(request.pin_token, pinauth_bytes));
}

// static
base::Optional<CredentialsMetadataResponse> CredentialsMetadataResponse::Parse(
    const base::Optional<cbor::Value>& cbor_response) {
  CredentialsMetadataResponse response;

  if (!cbor_response || !cbor_response->is_map()) {
    return base::nullopt;
  }
  const cbor::Value::MapValue& response_map = cbor_response->GetMap();

  auto it = response_map.find(cbor::Value(static_cast<int>(
      CredentialManagementResponseKey::kExistingResidentCredentialsCount)));
  if (it == response_map.end() || !it->second.is_unsigned()) {
    return base::nullopt;
  }
  const int64_t existing_count = it->second.GetUnsigned();
  if (existing_count > std::numeric_limits<size_t>::max()) {
    return base::nullopt;
  }
  response.num_existing_credentials = static_cast<size_t>(existing_count);

  it = response_map.find(cbor::Value(
      static_cast<int>(CredentialManagementResponseKey::
                           kMaxPossibleRemainingResidentCredentialsCount)));
  if (it == response_map.end() || !it->second.is_unsigned()) {
    return base::nullopt;
  }
  const int64_t remaining_count = it->second.GetUnsigned();
  if (remaining_count > std::numeric_limits<size_t>::max()) {
    return base::nullopt;
  }
  response.num_estimated_remaining_credentials =
      static_cast<size_t>(remaining_count);

  return response;
}

// static
base::Optional<EnumerateRPsResponse> EnumerateRPsResponse::Parse(
    const base::Optional<cbor::Value>& cbor_response,
    bool expect_rp_count) {
  if (!cbor_response || !cbor_response->is_map()) {
    return base::nullopt;
  }
  const cbor::Value::MapValue& response_map = cbor_response->GetMap();

  auto it = response_map.find(
      cbor::Value(static_cast<int>(CredentialManagementResponseKey::kRP)));
  if (it == response_map.end()) {
    return base::nullopt;
  }
  auto opt_rp = PublicKeyCredentialRpEntity::CreateFromCBORValue(it->second);
  if (!opt_rp) {
    return base::nullopt;
  }

  it = response_map.find(cbor::Value(
      static_cast<int>(CredentialManagementResponseKey::kRPIDHash)));
  if (it == response_map.end() || !it->second.is_bytestring()) {
    return base::nullopt;
  }
  const std::vector<uint8_t>& rp_id_hash_bytes = it->second.GetBytestring();
  if (rp_id_hash_bytes.size() != kRpIdHashLength) {
    return base::nullopt;
  }
  std::array<uint8_t, kRpIdHashLength> rp_id_hash;
  std::copy_n(rp_id_hash_bytes.begin(), kRpIdHashLength, rp_id_hash.begin());

  size_t rp_count = 0;
  if (!expect_rp_count) {
    if (response_map.find(cbor::Value(
            static_cast<int>(CredentialManagementResponseKey::kTotalRPs))) !=
        response_map.end()) {
      return base::nullopt;
    }
  } else {
    it = response_map.find(cbor::Value(
        static_cast<int>(CredentialManagementResponseKey::kTotalRPs)));
    if (it == response_map.end() || !it->second.is_unsigned() ||
        it->second.GetUnsigned() > std::numeric_limits<size_t>::max()) {
      return base::nullopt;
    }
    rp_count = static_cast<size_t>(it->second.GetUnsigned());
  }
  return EnumerateRPsResponse(std::move(*opt_rp), std::move(rp_id_hash),
                              rp_count);
}

EnumerateRPsResponse::EnumerateRPsResponse(EnumerateRPsResponse&&) = default;
EnumerateRPsResponse& EnumerateRPsResponse::operator=(EnumerateRPsResponse&&) =
    default;
EnumerateRPsResponse::~EnumerateRPsResponse() = default;
EnumerateRPsResponse::EnumerateRPsResponse(
    PublicKeyCredentialRpEntity rp_,
    std::array<uint8_t, kRpIdHashLength> rp_id_hash_,
    size_t rp_count_)
    : rp(std::move(rp_)),
      rp_id_hash(std::move(rp_id_hash_)),
      rp_count(rp_count_) {}

//  static
base::Optional<EnumerateCredentialsResponse>
EnumerateCredentialsResponse::Parse(
    const base::Optional<cbor::Value>& cbor_response,
    bool expect_credential_count) {
  if (!cbor_response || !cbor_response->is_map()) {
    return base::nullopt;
  }
  const cbor::Value::MapValue& response_map = cbor_response->GetMap();

  auto it = response_map.find(
      cbor::Value(static_cast<int>(CredentialManagementResponseKey::kUser)));
  if (it == response_map.end()) {
    return base::nullopt;
  }
  auto opt_user =
      PublicKeyCredentialUserEntity::CreateFromCBORValue(it->second);
  if (!opt_user) {
    return base::nullopt;
  }

  it = response_map.find(cbor::Value(
      static_cast<int>(CredentialManagementResponseKey::kCredentialID)));
  if (it == response_map.end() || !it->second.is_bytestring()) {
    return base::nullopt;
  }
  const std::vector<uint8_t>& credential_id = it->second.GetBytestring();

  // Ignore the public key's value.
  it = response_map.find(cbor::Value(
      static_cast<int>(CredentialManagementResponseKey::kPublicKey)));
  if (it == response_map.end() || !it->second.is_map()) {
    return base::nullopt;
  }

  size_t credential_count = 0;
  if (!expect_credential_count) {
    if (response_map.find(cbor::Value(static_cast<int>(
            CredentialManagementResponseKey::kTotalCredentials))) !=
        response_map.end()) {
      return base::nullopt;
    }
  } else {
    it = response_map.find(cbor::Value(
        static_cast<int>(CredentialManagementResponseKey::kTotalCredentials)));
    if (it == response_map.end() || !it->second.is_unsigned() ||
        it->second.GetUnsigned() > std::numeric_limits<size_t>::max()) {
      return base::nullopt;
    }
    credential_count = static_cast<size_t>(it->second.GetUnsigned());
  }
  return EnumerateCredentialsResponse(std::move(*opt_user), credential_id,
                                      credential_count);
}

EnumerateCredentialsResponse::EnumerateCredentialsResponse(
    EnumerateCredentialsResponse&&) = default;
EnumerateCredentialsResponse& EnumerateCredentialsResponse::operator=(
    EnumerateCredentialsResponse&&) = default;
EnumerateCredentialsResponse::~EnumerateCredentialsResponse() = default;
EnumerateCredentialsResponse::EnumerateCredentialsResponse(
    PublicKeyCredentialUserEntity user_,
    std::vector<uint8_t> credential_id_,
    size_t credential_count_)
    : user(std::move(user_)),
      credential_id(std::move(credential_id_)),
      credential_count(credential_count_) {}

AggregatedEnumerateCredentialsResponse::AggregatedEnumerateCredentialsResponse(
    PublicKeyCredentialRpEntity rp_)
    : rp(std::move(rp_)), credentials() {}
AggregatedEnumerateCredentialsResponse::
    ~AggregatedEnumerateCredentialsResponse() = default;

}  // namespace device
