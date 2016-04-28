// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_FUZZED_DATA_PROVIDER_H
#define NET_BASE_FUZZED_DATA_PROVIDER_H

#include <stdint.h>

#include "base/macros.h"
#include "base/strings/string_piece.h"

namespace net {

// Utility class to break up fuzzer input for multiple consumers. Whenever run
// on the same input, provides the same output, as long as its methods are
// called in the same order, with the same arguments.
class FuzzedDataProvider {
 public:
  // |data| is an array of length |size| that the FuzzedDataProvider wraps to
  // provide more granular access. |data| must outlive the FuzzedDataProvider.
  FuzzedDataProvider(const uint8_t* data, size_t size);
  ~FuzzedDataProvider();

  // Returns a StringPiece containing |length| bytes of the input data. If fewer
  // than that many bytes remain, returns a shorter StringPiece containing all
  // of the data that's left. The data pointed at by the returned StringPiece
  // must not be used after the FuzzedDataProvider is destroyed.
  base::StringPiece ConsumeBytes(size_t bytes);

  // Returns a value from |min| to |max| inclusive, extracting a value from the
  // input data in some unspecified manner. Value may not be uniformly
  // distributed in the given range. If there's no input data left, always
  // returns |min|. |min| must be less than or equal to |max|.
  uint32_t ConsumeValueInRange(uint32_t min, uint32_t max);

  // Returns a value with the specified number of bits of data. Returns 0 if
  // there's no input data left. |num_bits| must non-zero, and at most 32.
  // Basically the same as ConsumeValueInRange((1 << num_bits) - 1);
  uint32_t ConsumeBits(size_t num_bits);

  // Same as ConsumeBits(1), but returns a bool. Returns false when there's no
  // data remaining.
  bool ConsumeBool();

 private:
  base::StringPiece remaining_data_;

  DISALLOW_COPY_AND_ASSIGN(FuzzedDataProvider);
};

}  // namespace net

#endif  // NET_BASE_FUZZED_DATA_PROVIDER_H
