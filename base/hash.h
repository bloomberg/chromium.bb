// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_HASH_H_
#define BASE_HASH_H_

#include <limits>
#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/logging.h"

namespace base {

// WARNING: This hash function should not be used for any cryptographic purpose.
BASE_EXPORT uint32 SuperFastHash(const char* data, int len);

// Computes the hash of a string |key| of a given |length|. |key| does not need
// to be null-terminated, and may contain null bytes.
// WARNING: This hash function should not be used for any cryptographic purpose.
inline uint32 Hash(const char* key, size_t length) {
  if (length > static_cast<size_t>(std::numeric_limits<int>::max())) {
    NOTREACHED();
    return 0;
  }
  return SuperFastHash(key, static_cast<int>(length));
}

// Computes the hash of a string |key|.
// WARNING: This hash function should not be used for any cryptographic purpose.
inline uint32 Hash(const std::string& key) {
  return Hash(key.data(), key.size());
}

}  // namespace base

#endif  // BASE_HASH_H_
