// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_data.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "content/browser/webauth/attested_credential_data.h"
#include "content/browser/webauth/authenticator_utils.h"
#include "crypto/sha2.h"

namespace content {

// static
std::unique_ptr<AuthenticatorData> AuthenticatorData::Create(
    std::string client_data_json,
    Flags flags,
    std::vector<uint8_t> counter,
    std::unique_ptr<AttestedCredentialData> data) {
  base::DictionaryValue* client_data_dictionary;
  std::unique_ptr<base::Value> client_data =
      base::JSONReader::Read(client_data_json);
  client_data->GetAsDictionary(&client_data_dictionary);
  std::string relying_party_id =
      client_data_dictionary->FindKey(authenticator_utils::kOriginKey)
          ->GetString();

  return std::make_unique<AuthenticatorData>(
      std::move(relying_party_id), flags, std::move(counter), std::move(data));
}

AuthenticatorData::AuthenticatorData(
    std::string relying_party_id,
    Flags flags,
    std::vector<uint8_t> counter,
    std::unique_ptr<AttestedCredentialData> data)
    : relying_party_id_(std::move(relying_party_id)),
      flags_(flags),
      counter_(std::move(counter)),
      attested_data_(std::move(data)) {
  CHECK_EQ(counter_.size(), 4u);
}

std::vector<uint8_t> AuthenticatorData::SerializeToByteArray() {
  std::vector<uint8_t> authenticator_data;
  std::vector<uint8_t> rp_id_hash(crypto::kSHA256Length);
  crypto::SHA256HashString(relying_party_id_, rp_id_hash.data(),
                           rp_id_hash.size());
  authenticator_utils::Append(&authenticator_data, rp_id_hash);
  authenticator_data.insert(authenticator_data.end(), flags_);
  authenticator_utils::Append(&authenticator_data, counter_);
  std::vector<uint8_t> attestation_bytes = attested_data_->SerializeAsBytes();
  authenticator_utils::Append(&authenticator_data, attestation_bytes);

  return authenticator_data;
}

AuthenticatorData::~AuthenticatorData() {}

}  // namespace content
