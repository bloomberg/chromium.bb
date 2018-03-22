// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/attested_credential_data.h"

#include <stddef.h>

#include <utility>

#include "base/numerics/safe_math.h"
#include "device/fido/opaque_public_key.h"
#include "device/fido/public_key.h"
#include "device/fido/u2f_parsing_utils.h"

namespace device {

namespace {

constexpr size_t kAaguidLength = 16;

// Number of bytes used to represent length of credential ID.
constexpr size_t kCredentialIdLengthLength = 2;

}  // namespace

// static
base::Optional<AttestedCredentialData>
AttestedCredentialData::DecodeFromCtapResponse(
    base::span<const uint8_t> buffer) {
  if (buffer.size() < kAaguidLength + kCredentialIdLengthLength)
    return base::nullopt;

  auto aaguid = u2f_parsing_utils::Extract(buffer, 0, kAaguidLength);
  auto credential_id_length_span = u2f_parsing_utils::ExtractSpan(
      buffer, kAaguidLength, kCredentialIdLengthLength);

  if (aaguid.empty() || credential_id_length_span.empty())
    return base::nullopt;

  static_assert(kCredentialIdLengthLength == 2u, "L must be 2 bytes");
  const size_t credential_id_length =
      credential_id_length_span[0] << 8 | credential_id_length_span[1];

  auto credential_id = u2f_parsing_utils::Extract(
      buffer, kAaguidLength + kCredentialIdLengthLength, credential_id_length);
  if (credential_id.empty())
    return base::nullopt;

  DCHECK_LE(kAaguidLength + kCredentialIdLengthLength + credential_id_length,
            buffer.size());
  auto credential_public_key_data =
      std::make_unique<OpaquePublicKey>(buffer.subspan(
          kAaguidLength + kCredentialIdLengthLength + credential_id_length));

  return AttestedCredentialData(
      std::move(aaguid),
      std::vector<uint8_t>(credential_id_length_span.begin(),
                           credential_id_length_span.end()),
      std::move(credential_id), std::move(credential_public_key_data));
}

// static
base::Optional<AttestedCredentialData>
AttestedCredentialData::CreateFromU2fRegisterResponse(
    base::span<const uint8_t> u2f_data,
    std::vector<uint8_t> aaguid,
    std::unique_ptr<PublicKey> public_key) {
  // TODO(crbug/799075): Introduce a CredentialID class to do this extraction.
  // Extract the length of the credential (i.e. of the U2FResponse key
  // handle). Length is big endian.
  std::vector<uint8_t> extracted_length = u2f_parsing_utils::Extract(
      u2f_data, u2f_parsing_utils::kU2fResponseKeyHandleLengthPos, 1);

  if (extracted_length.empty()) {
    return base::nullopt;
  }

  // Note that U2F responses only use one byte for length.
  std::vector<uint8_t> credential_id_length = {0, extracted_length[0]};

  // Extract the credential id (i.e. key handle).
  std::vector<uint8_t> credential_id = u2f_parsing_utils::Extract(
      u2f_data, u2f_parsing_utils::kU2fResponseKeyHandleStartPos,
      base::strict_cast<size_t>(credential_id_length[1]));

  if (credential_id.empty()) {
    return base::nullopt;
  }

  return AttestedCredentialData(
      std::move(aaguid), std::move(credential_id_length),
      std::move(credential_id), std::move(public_key));
}

AttestedCredentialData::AttestedCredentialData(
    std::vector<uint8_t> aaguid,
    std::vector<uint8_t> length,
    std::vector<uint8_t> credential_id,
    std::unique_ptr<PublicKey> public_key)
    : aaguid_(std::move(aaguid)),
      credential_id_length_(std::move(length)),
      credential_id_(std::move(credential_id)),
      public_key_(std::move(public_key)) {}

AttestedCredentialData::AttestedCredentialData(AttestedCredentialData&& other) =
    default;

AttestedCredentialData& AttestedCredentialData::operator=(
    AttestedCredentialData&& other) = default;

AttestedCredentialData::~AttestedCredentialData() = default;

void AttestedCredentialData::DeleteAaguid() {
  aaguid_ = std::vector<uint8_t>(kAaguidLength, 0);
}

std::vector<uint8_t> AttestedCredentialData::SerializeAsBytes() const {
  std::vector<uint8_t> attestation_data;
  u2f_parsing_utils::Append(&attestation_data, aaguid_);
  u2f_parsing_utils::Append(&attestation_data, credential_id_length_);
  u2f_parsing_utils::Append(&attestation_data, credential_id_);
  u2f_parsing_utils::Append(&attestation_data, public_key_->EncodeAsCOSEKey());
  return attestation_data;
}

}  // namespace device
