// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/origin_trials/trial_token.h"

#include <openssl/curve25519.h>

#include <vector>

#include "base/base64.h"
#include "base/big_endian.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/values.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// Version is a 1-byte field at offset 0.
const size_t kVersionOffset = 0;
const size_t kVersionSize = 1;

// These constants define the Version 2 field sizes and offsets.
const size_t kSignatureOffset = kVersionOffset + kVersionSize;
const size_t kSignatureSize = 64;
const size_t kPayloadLengthOffset = kSignatureOffset + kSignatureSize;
const size_t kPayloadLengthSize = 4;
const size_t kPayloadOffset = kPayloadLengthOffset + kPayloadLengthSize;

// Version 2 is the only token version currently supported. Version 1 was
// introduced in Chrome M50, and removed in M51. There were no experiments
// enabled in the stable M50 release which would have used those tokens.
const uint8_t kVersion2 = 2;

}  // namespace

TrialToken::~TrialToken() {}

// static
std::unique_ptr<TrialToken> TrialToken::From(const std::string& token_text,
                                             base::StringPiece public_key) {
  std::unique_ptr<std::string> token_payload = Extract(token_text, public_key);
  if (!token_payload) {
    return nullptr;
  }
  return Parse(*token_payload);
}

bool TrialToken::IsValidForFeature(const url::Origin& origin,
                                   base::StringPiece feature_name,
                                   const base::Time& now) const {
  return ValidateOrigin(origin) && ValidateFeatureName(feature_name) &&
         ValidateDate(now);
}

std::unique_ptr<std::string> TrialToken::Extract(
    const std::string& token_payload,
    base::StringPiece public_key) {
  if (token_payload.empty()) {
    return nullptr;
  }

  // Token is base64-encoded; decode first.
  std::string token_contents;
  if (!base::Base64Decode(token_payload, &token_contents)) {
    return nullptr;
  }

  // Only version 2 currently supported.
  if (token_contents.length() < (kVersionOffset + kVersionSize)) {
    return nullptr;
  }
  uint8_t version = token_contents[kVersionOffset];
  if (version != kVersion2) {
    return nullptr;
  }

  // Token must be large enough to contain a version, signature, and payload
  // length.
  if (token_contents.length() < (kPayloadLengthOffset + kPayloadLengthSize)) {
    return nullptr;
  }

  // Extract the length of the signed data (Big-endian).
  uint32_t payload_length;
  base::ReadBigEndian(&(token_contents[kPayloadLengthOffset]), &payload_length);

  // Validate that the stated length matches the actual payload length.
  if (payload_length != token_contents.length() - kPayloadOffset) {
    return nullptr;
  }

  // Extract the version-specific contents of the token.
  const char* token_bytes = token_contents.data();
  base::StringPiece version_piece(token_bytes + kVersionOffset, kVersionSize);
  base::StringPiece signature(token_bytes + kSignatureOffset, kSignatureSize);
  base::StringPiece payload_piece(token_bytes + kPayloadLengthOffset,
                                  kPayloadLengthSize + payload_length);

  // The data which is covered by the signature is (version + length + payload).
  std::string signed_data =
      version_piece.as_string() + payload_piece.as_string();

  // Validate the signature on the data before proceeding.
  if (!TrialToken::ValidateSignature(signature, signed_data, public_key)) {
    return nullptr;
  }

  // Return just the payload, as a new string.
  return base::WrapUnique(
      new std::string(token_contents, kPayloadOffset, payload_length));
}

std::unique_ptr<TrialToken> TrialToken::Parse(const std::string& token_json) {
  std::unique_ptr<base::DictionaryValue> datadict =
      base::DictionaryValue::From(base::JSONReader::Read(token_json));
  if (!datadict) {
    return nullptr;
  }

  std::string origin_string;
  std::string feature_name;
  int expiry_timestamp = 0;
  datadict->GetString("origin", &origin_string);
  datadict->GetString("feature", &feature_name);
  datadict->GetInteger("expiry", &expiry_timestamp);

  // Ensure that the origin is a valid (non-unique) origin URL.
  url::Origin origin = url::Origin(GURL(origin_string));
  if (origin.unique()) {
    return nullptr;
  }

  // Ensure that the feature name is a valid string.
  if (feature_name.empty()) {
    return nullptr;
  }

  // Ensure that the expiry timestamp is a valid (positive) integer.
  if (expiry_timestamp <= 0) {
    return nullptr;
  }

  return base::WrapUnique(
      new TrialToken(origin, feature_name, expiry_timestamp));
}

bool TrialToken::ValidateOrigin(const url::Origin& origin) const {
  return origin == origin_;
}

bool TrialToken::ValidateFeatureName(base::StringPiece feature_name) const {
  return feature_name == feature_name_;
}

bool TrialToken::ValidateDate(const base::Time& now) const {
  return expiry_time_ > now;
}

// static
bool TrialToken::ValidateSignature(base::StringPiece signature,
                                   const std::string& data,
                                   base::StringPiece public_key) {
  // Public key must be 32 bytes long for Ed25519.
  CHECK_EQ(public_key.length(), 32UL);

  // Signature must be 64 bytes long.
  if (signature.length() != 64) {
    return false;
  }

  int result = ED25519_verify(
      reinterpret_cast<const uint8_t*>(data.data()), data.length(),
      reinterpret_cast<const uint8_t*>(signature.data()),
      reinterpret_cast<const uint8_t*>(public_key.data()));
  return (result != 0);
}

TrialToken::TrialToken(const url::Origin& origin,
                       const std::string& feature_name,
                       uint64_t expiry_timestamp)
    : origin_(origin),
      feature_name_(feature_name),
      expiry_time_(base::Time::FromDoubleT(expiry_timestamp)) {}

}  // namespace content
