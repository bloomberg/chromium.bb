// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/bloom_filter.h"

#include <stddef.h>

#include "base/logging.h"
#include "third_party/smhasher/src/City.h"

namespace rappor {

namespace {

uint32_t ComputeHash(const std::string& str, uint32_t seed) {
    // Using CityHash here because we have support for it in Dremel.  Many hash
    // functions, such as MD5, SHA1, or Murmur, would probably also work.
    return CityHash64WithSeed(str.data(), str.size(), seed);
}

}  // namespace

BloomFilter::BloomFilter(uint32_t bytes_size,
                         uint32_t hash_function_count,
                         uint32_t hash_seed_offset)
    : bytes_(bytes_size),
      hash_function_count_(hash_function_count),
      hash_seed_offset_(hash_seed_offset) {
  DCHECK_GT(bytes_size, 0u);
}

BloomFilter::~BloomFilter() {}

void BloomFilter::SetString(const std::string& str) {
  for (size_t i = 0; i < bytes_.size(); ++i) {
    bytes_[i] = 0;
  }
  for (size_t i = 0; i < hash_function_count_; ++i) {
    uint32_t index = ComputeHash(str, hash_seed_offset_ + i);
    // Note that the "bytes" are uint8_t, so they are always 8-bits.
    uint32_t byte_index = (index / 8) % bytes_.size();
    uint32_t bit_index = index % 8;
    bytes_[byte_index] |= 1 << bit_index;
  }
}

namespace internal {

uint64_t GetBloomBits(uint32_t bytes_size,
                      uint32_t hash_function_count,
                      uint32_t hash_seed_offset,
                      const std::string& str) {
  // Make sure result fits in uint64_t.
  DCHECK_LE(bytes_size, 8u);
  uint64_t output = 0;
  const uint32_t bits_size = bytes_size * 8;
  for (size_t i = 0; i < hash_function_count; ++i) {
    uint32_t index = ComputeHash(str, hash_seed_offset + i);
    output |= 1ULL << uint64_t(index % bits_size);
  }
  return output;
}

}  // namespace internal

}  // namespace rappor
