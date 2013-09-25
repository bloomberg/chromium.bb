// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/jtl_foundation.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace jtl_foundation {

Hasher::Hasher(const std::string& seed) : hmac_(crypto::HMAC::SHA256) {
  if (!hmac_.Init(seed))
    NOTREACHED();
}

Hasher::~Hasher() {}

std::string Hasher::GetHash(const std::string& input) const {
  if (cached_hashes_.find(input) == cached_hashes_.end()) {
    // Calculate value.
    unsigned char digest[kHashSizeInBytes];
    if (!hmac_.Sign(input, digest, kHashSizeInBytes)) {
      NOTREACHED();
      return std::string();
    }
    // Instead of using the full SHA256, we only use the hex encoding of the
    // first 16 bytes.
    cached_hashes_[input] = base::HexEncode(digest, kHashSizeInBytes / 2);
    DCHECK_EQ(kHashSizeInBytes, cached_hashes_[input].size());
  }
  return cached_hashes_[input];
}

}  // namespace jtl_foundation
