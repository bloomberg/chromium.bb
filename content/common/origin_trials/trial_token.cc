// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/origin_trials/trial_token.h"

#include <openssl/curve25519.h>

#include <vector>

#include "base/base64.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "url/origin.h"

namespace content {

namespace {

// This is the default public key used for validating signatures.
// TODO(iclelland): Move this to the embedder, and provide a mechanism to allow
// for multiple signing keys. https://crbug.com/543220
static const uint8_t kPublicKey[] = {
    0x7c, 0xc4, 0xb8, 0x9a, 0x93, 0xba, 0x6e, 0xe2, 0xd0, 0xfd, 0x03,
    0x1d, 0xfb, 0x32, 0x66, 0xc7, 0x3b, 0x72, 0xfd, 0x54, 0x3a, 0x07,
    0x51, 0x14, 0x66, 0xaa, 0x02, 0x53, 0x4e, 0x33, 0xa1, 0x15,
};

const char* kFieldSeparator = "|";

}  // namespace

TrialToken::~TrialToken() {}

scoped_ptr<TrialToken> TrialToken::Parse(const std::string& token_text) {
  if (token_text.empty()) {
    return nullptr;
  }

  // A valid token should resemble:
  // signature|origin|feature_name|expiry_timestamp
  // TODO(iclelland): Add version code to token format to identify key algo
  // https://crbug.com/570684
  std::vector<std::string> parts = SplitString(
      token_text, kFieldSeparator, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 4) {
    return nullptr;
  }

  const std::string& signature = parts[0];
  const std::string& origin_string = parts[1];
  const std::string& feature_name = parts[2];
  const std::string& expiry_string = parts[3];

  uint64_t expiry_timestamp;
  if (!base::StringToUint64(expiry_string, &expiry_timestamp)) {
    return nullptr;
  }

  // Ensure that the origin is a valid (non-unique) origin URL
  GURL origin_url(origin_string);
  if (url::Origin(origin_url).unique()) {
    return nullptr;
  }

  // signed data is (origin + "|" + feature_name + "|" + expiry).
  std::string data = token_text.substr(signature.length() + 1);

  return make_scoped_ptr(new TrialToken(signature, data, origin_url,
                                        feature_name, expiry_timestamp));
}

TrialToken::TrialToken(const std::string& signature,
                       const std::string& data,
                       const GURL& origin,
                       const std::string& feature_name,
                       uint64_t expiry_timestamp)
    : signature_(signature),
      data_(data),
      origin_(origin),
      feature_name_(feature_name),
      expiry_timestamp_(expiry_timestamp) {}

bool TrialToken::IsAppropriate(const std::string& origin,
                               const std::string& feature_name) const {
  return ValidateOrigin(origin) && ValidateFeatureName(feature_name);
}

bool TrialToken::IsValid(const base::Time& now) const {
  // TODO(iclelland): Allow for multiple signing keys, and iterate over all
  // active keys here. https://crbug.com/543220
  return ValidateDate(now) &&
         ValidateSignature(base::StringPiece(
             reinterpret_cast<const char*>(kPublicKey), arraysize(kPublicKey)));
}

bool TrialToken::ValidateOrigin(const std::string& origin) const {
  return GURL(origin) == origin_;
}

bool TrialToken::ValidateFeatureName(const std::string& feature_name) const {
  return feature_name == feature_name_;
}

bool TrialToken::ValidateDate(const base::Time& now) const {
  base::Time expiry_time = base::Time::FromDoubleT((double)expiry_timestamp_);
  return expiry_time > now;
}

bool TrialToken::ValidateSignature(const base::StringPiece& public_key) const {
  return ValidateSignature(signature_, data_, public_key);
}

// static
bool TrialToken::ValidateSignature(const std::string& signature_text,
                                   const std::string& data,
                                   const base::StringPiece& public_key) {
  // Public key must be 32 bytes long for Ed25519.
  CHECK_EQ(public_key.length(), 32UL);

  std::string signature;
  // signature_text is base64-encoded; decode first.
  if (!base::Base64Decode(signature_text, &signature)) {
    return false;
  }

  // Signature must be 64 bytes long
  if (signature.length() != 64) {
    return false;
  }

  int result = ED25519_verify(
      reinterpret_cast<const uint8_t*>(data.data()), data.length(),
      reinterpret_cast<const uint8_t*>(signature.data()),
      reinterpret_cast<const uint8_t*>(public_key.data()));
  return (result != 0);
}

}  // namespace content
