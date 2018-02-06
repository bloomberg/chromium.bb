// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/collected_client_data.h"

#include <utility>

#include "base/base64url.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
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
                             std::move(relying_party_id), "unused");
}

CollectedClientData::CollectedClientData() = default;

CollectedClientData::CollectedClientData(std::string type,
                                         std::string base64_encoded_challenge,
                                         std::string origin,
                                         std::string token_binding_id)
    : type_(std::move(type)),
      base64_encoded_challenge_(std::move(base64_encoded_challenge)),
      origin_(std::move(origin)),
      token_binding_id_(std::move(token_binding_id)) {}

CollectedClientData::CollectedClientData(CollectedClientData&& other) = default;
CollectedClientData& CollectedClientData::operator=(
    CollectedClientData&& other) = default;

CollectedClientData::~CollectedClientData() = default;

std::string CollectedClientData::SerializeToJson() const {
  base::DictionaryValue client_data;
  client_data.SetKey(kTypeKey, base::Value(type_));
  client_data.SetKey(kChallengeKey, base::Value(base64_encoded_challenge_));

  if (base::RandDouble() < 0.2) {
    // An extra key is sometimes added to ensure that RPs do not make
    // unreasonably specific assumptions about the clientData JSON. This is
    // done in the fashion of
    // https://tools.ietf.org/html/draft-davidben-tls-grease-01
    client_data.SetKey("new_keys_may_be_added_here",
                       base::Value("do not compare clientDataJSON against a "
                                   "template. See https://goo.gl/yabPex"));
  }

  // The serialization of callerOrigin.
  client_data.SetKey(kOriginKey, base::Value(origin_));

  client_data.SetKey(kTokenBindingKey, base::Value(token_binding_id_));

  std::string json;
  base::JSONWriter::Write(client_data, &json);
  return json;
}

}  // namespace content
