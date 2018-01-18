// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/collected_client_data.h"

#include <utility>

#include "base/base64url.h"
#include "base/json/json_writer.h"
#include "base/strings/string_piece.h"
#include "base/values.h"

namespace content {

namespace client_data {
const char kCreateType[] = "webauthn.create";
const char kGetType[] = "webauthn.get";
}  // namespace client_data

namespace {
constexpr char kTypeKey[] = "type";
constexpr char kChallengeKey[] = "challenge";
constexpr char kOriginKey[] = "origin";
constexpr char kHashAlgorithm[] = "hashAlgorithm";
constexpr char kTokenBindingKey[] = "tokenBinding";
}  // namespace

// static
CollectedClientData CollectedClientData::Create(
    std::string type,
    std::string relying_party_id,
    base::span<const uint8_t> challenge) {
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
  return CollectedClientData(std::move(type), std::move(encoded_challenge),
                             std::move(relying_party_id), "SHA-256", "unused");
}

CollectedClientData::CollectedClientData() = default;

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

CollectedClientData::CollectedClientData(CollectedClientData&& other) = default;
CollectedClientData& CollectedClientData::operator=(
    CollectedClientData&& other) = default;

CollectedClientData::~CollectedClientData() = default;

std::string CollectedClientData::SerializeToJson() const {
  base::DictionaryValue client_data;
  client_data.SetKey(kTypeKey, base::Value(type_));
  client_data.SetKey(kChallengeKey, base::Value(base64_encoded_challenge_));

  // The serialization of callerOrigin.
  client_data.SetKey(kOriginKey, base::Value(origin_));

  // The recognized algorithm name of the hash algorithm selected by the client
  // for generating the hash of the serialized client data.
  client_data.SetKey(kHashAlgorithm, base::Value(hash_algorithm_));

  client_data.SetKey(kTokenBindingKey, base::Value(token_binding_id_));

  std::string json;
  base::JSONWriter::Write(client_data, &json);
  return json;
}

}  // namespace content
