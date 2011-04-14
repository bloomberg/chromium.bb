// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/hmac.h"

#include <CommonCrypto/CommonHMAC.h>

#include "base/logging.h"

namespace crypto {

struct HMACPlatformData {
  std::string key_;
};

HMAC::HMAC(HashAlgorithm hash_alg)
    : hash_alg_(hash_alg), plat_(new HMACPlatformData()) {
  // Only SHA-1 and SHA-256 hash algorithms are supported now.
  DCHECK(hash_alg_ == SHA1 || hash_alg_ == SHA256);
}

bool HMAC::Init(const unsigned char *key, int key_length) {
  if (!plat_->key_.empty()) {
    // Init must not be called more than once on the same HMAC object.
    NOTREACHED();
    return false;
  }

  plat_->key_.assign(reinterpret_cast<const char*>(key), key_length);

  return true;
}

HMAC::~HMAC() {
  // Zero out key copy.
  plat_->key_.assign(plat_->key_.length(), std::string::value_type());
  plat_->key_.clear();
  plat_->key_.reserve(0);
}

bool HMAC::Sign(const std::string& data,
                unsigned char* digest,
                int digest_length) {
  CCHmacAlgorithm algorithm;
  int algorithm_digest_length;
  switch (hash_alg_) {
    case SHA1:
      algorithm = kCCHmacAlgSHA1;
      algorithm_digest_length = CC_SHA1_DIGEST_LENGTH;
      break;
    case SHA256:
      algorithm = kCCHmacAlgSHA256;
      algorithm_digest_length = CC_SHA256_DIGEST_LENGTH;
      break;
    default:
      NOTREACHED();
      return false;
  }

  if (digest_length < algorithm_digest_length) {
    NOTREACHED();
    return false;
  }

  CCHmac(algorithm,
         plat_->key_.data(), plat_->key_.length(), data.data(), data.length(),
         digest);

  return true;
}

}  // namespace crypto
