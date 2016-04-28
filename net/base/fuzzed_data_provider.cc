// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/fuzzed_data_provider.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"

namespace net {

FuzzedDataProvider::FuzzedDataProvider(const uint8_t* data, size_t size)
    : remaining_data_(reinterpret_cast<const char*>(data), size) {}

FuzzedDataProvider::~FuzzedDataProvider() {}

base::StringPiece FuzzedDataProvider::ConsumeBytes(size_t length) {
  length = std::min(length, remaining_data_.length());
  base::StringPiece result(remaining_data_.data(), length);
  remaining_data_ = remaining_data_.substr(length);
  return result;
}

uint32_t FuzzedDataProvider::ConsumeValueInRange(uint32_t min, uint32_t max) {
  CHECK_LE(min, max);

  uint32_t range = max - min;
  uint32_t offset = 0;
  uint32_t result = 0;

  while ((range >> offset) > 0 && !remaining_data_.empty()) {
    // Pull bytes off the end of the seed data. Experimentally, this seems to
    // allow the fuzzer to more easily explore the input space. This makes
    // sense, since it works by modifying inputs that caused new code to run,
    // and this data is often used to encode length of data read by
    // ConsumeBytes. Separating out read lengths makes it easier modify the
    // contents of the data that is actually read.
    uint8_t next_byte = remaining_data_.data()[remaining_data_.length() - 1];
    result = (result << 8) | next_byte;
    remaining_data_ = remaining_data_.substr(0, remaining_data_.length() - 1);
    offset += 8;
  }

  // Avoid division by 0, in the case |range + 1| results in overflow.
  if (range == std::numeric_limits<uint32_t>::max())
    return result;

  return min + result % (range + 1);
}

uint32_t FuzzedDataProvider::ConsumeBits(size_t num_bits) {
  CHECK_NE(0u, num_bits);
  CHECK_LE(num_bits, 32u);

  if (num_bits == 32)
    return ConsumeValueInRange(0, std::numeric_limits<uint32_t>::max());
  return ConsumeValueInRange(0, (1 << num_bits) - 1);
}

bool FuzzedDataProvider::ConsumeBool() {
  // Double negation so this returns false once there's no more data.
  return !!ConsumeBits(1);
}

}  // namespace net
