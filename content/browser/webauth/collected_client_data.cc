// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/collected_client_data.h"

#include <utility>

#include "base/base64url.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "content/browser/webauth/authenticator_utils.h"

namespace content {

// static
std::unique_ptr<CollectedClientData> CollectedClientData::Create(
    std::string type,
    std::string relying_party_id,
    std::vector<uint8_t> challenge) {
  //  The base64url encoding of options.challenge.
  std::string encoded_challenge;
  base::Base64UrlEncode(
      base::StringPiece(reinterpret_cast<const char*>(challenge.data()),
                        challenge.size()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &encoded_challenge);

  // TokenBinding is present and set to the constant "unused" if the browser
  // supports Token Binding, but is not using it to talk to the origin.
  // TODO(kpaulhamus): Fetch and add the Token Binding ID public key used to
  // communicate with the origin.
  return std::make_unique<CollectedClientData>(
      std::move(type), std::move(encoded_challenge),
      std::move(relying_party_id), "SHA-256", "unused");
}

CollectedClientData::CollectedClientData(std::string type,
                                         std::string base64_encoded_challenge,
                                         std::string origin,
                                         std::string hash_algorithm,
                                         std::string token_binding_id)
    : type_(std::move(type)),
      base64_encoded_challenge_(std::move(base64_encoded_challenge)),
      origin_(std::move(origin)),
      hash_algorithm_(std::move(hash_algorithm)),
      token_binding_id_(std::move(token_binding_id)) {}

std::string CollectedClientData::SerializeToJson() {
  base::DictionaryValue client_data;
  client_data.SetString(authenticator_utils::kTypeKey, type_);
  client_data.SetString(authenticator_utils::kChallengeKey,
                        base64_encoded_challenge_);

  // The serialization of callerOrigin.
  client_data.SetString(authenticator_utils::kOriginKey, origin_);

  // The recognized algorithm name of the hash algorithm selected by the client
  // for generating the hash of the serialized client data.
  client_data.SetString(authenticator_utils::kHashAlgorithm, hash_algorithm_);

  client_data.SetString(authenticator_utils::kTokenBindingKey,
                        token_binding_id_);

  std::string json;
  base::JSONWriter::Write(client_data, &json);
  return json;
}

CollectedClientData::~CollectedClientData() {}

}  // namespace content
