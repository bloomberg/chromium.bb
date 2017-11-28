// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/attested_credential_data.h"

#include <utility>

#include "base/numerics/safe_math.h"
#include "content/browser/webauth/authenticator_utils.h"

namespace content {

// static
std::unique_ptr<AttestedCredentialData>
AttestedCredentialData::CreateFromU2fRegisterResponse(
    const std::vector<uint8_t>& u2f_data,
    std::vector<uint8_t> aaguid,
    std::unique_ptr<PublicKey> public_key) {
  // Extract the length of the credential (i.e. of the U2FResponse key
  // handle). Length is big endian.
  std::vector<uint8_t> credential_id_length(2, 0);
  std::vector<uint8_t> extracted_length = authenticator_utils::Extract(
      u2f_data, authenticator_utils::kU2fResponseLengthPos, 1);

  // Note that U2F responses only use one byte for length.
  credential_id_length[1] = extracted_length[0];

  // Extract the credential id (i.e. key handle).
  std::vector<uint8_t> credential_id = authenticator_utils::Extract(
      u2f_data, authenticator_utils::kU2fResponseKeyHandleStartPos,
      base::strict_cast<size_t>(credential_id_length[1]));

  return std::make_unique<AttestedCredentialData>(
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

std::vector<uint8_t> AttestedCredentialData::SerializeAsBytes() {
  std::vector<uint8_t> attestation_data;
  std::vector<uint8_t> cbor_encoded_key = public_key_->EncodeAsCBOR();
  authenticator_utils::Append(&attestation_data, aaguid_);
  authenticator_utils::Append(&attestation_data, credential_id_length_);
  authenticator_utils::Append(&attestation_data, credential_id_);
  authenticator_utils::Append(&attestation_data, cbor_encoded_key);
  return attestation_data;
}

AttestedCredentialData::~AttestedCredentialData() {}

}  // namespace content
