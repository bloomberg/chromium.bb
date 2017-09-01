// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hash.h"

// Definition in base/third_party/superfasthash/superfasthash.c. (Third-party
// code did not come with its own header file, so declaring the function here.)
// Note: This algorithm is also in Blink under Source/wtf/StringHasher.h.
extern "C" uint32_t SuperFastHash(const char* data, int len);

namespace base {

uint32_t Hash(const void* data, size_t length) {
  // Currently our in-memory hash is the same as the persistent hash. The
  // split between in-memory and persistent hash functions is maintained to
  // allow the in-memory hash function to be updated in the future.
  return PersistentHash(data, length);
}

uint32_t Hash(const std::string& str) {
  return PersistentHash(str.data(), str.size());
}

uint32_t Hash(const string16& str) {
  return PersistentHash(str.data(), str.size() * sizeof(char16));
}

uint32_t PersistentHash(const void* data, size_t length) {
  // This hash function must not change, since it is designed to be persistable
  // to disk.
  if (length > static_cast<size_t>(std::numeric_limits<int>::max())) {
    NOTREACHED();
    return 0;
  }
  return ::SuperFastHash(reinterpret_cast<const char*>(data),
                         static_cast<int>(length));
}

uint32_t PersistentHash(const std::string& str) {
  return PersistentHash(str.data(), str.size());
}

}  // namespace base
