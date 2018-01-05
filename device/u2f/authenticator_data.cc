// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/authenticator_data.h"

#include <utility>

#include "crypto/sha2.h"
#include "device/u2f/u2f_parsing_utils.h"

namespace device {

// static
AuthenticatorData AuthenticatorData::Create(std::string relying_party_id,
                                            uint8_t flags,
                                            std::vector<uint8_t> counter,
                                            AttestedCredentialData data) {
  return AuthenticatorData(std::move(relying_party_id), flags,
                           std::move(counter), std::move(data));
}

AuthenticatorData::AuthenticatorData(std::string relying_party_id,
                                     uint8_t flags,
                                     std::vector<uint8_t> counter,
                                     AttestedCredentialData data)
    : relying_party_id_(std::move(relying_party_id)),
      flags_(flags),
      counter_(std::move(counter)),
      attested_data_(std::move(data)) {
  // TODO(kpaulhamus): use std::array for these small, fixed-sized vectors.
  CHECK_EQ(counter_.size(), 4u);
}

AuthenticatorData::AuthenticatorData(AuthenticatorData&& other) = default;
AuthenticatorData& AuthenticatorData::operator=(AuthenticatorData&& other) =
    default;

AuthenticatorData::~AuthenticatorData() = default;

std::vector<uint8_t> AuthenticatorData::SerializeToByteArray() const {
  std::vector<uint8_t> authenticator_data;
  std::vector<uint8_t> rp_id_hash(crypto::kSHA256Length);
  crypto::SHA256HashString(relying_party_id_, rp_id_hash.data(),
                           rp_id_hash.size());
  u2f_parsing_utils::Append(&authenticator_data, rp_id_hash);
  authenticator_data.insert(authenticator_data.end(), flags_);
  u2f_parsing_utils::Append(&authenticator_data, counter_);
  std::vector<uint8_t> attestation_bytes = attested_data_.SerializeAsBytes();
  u2f_parsing_utils::Append(&authenticator_data, attestation_bytes);

  return authenticator_data;
}

}  // namespace device
