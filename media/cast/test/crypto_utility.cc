// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "media/cast/test/crypto_utility.h"

namespace media {
namespace cast {

std::string ConvertFromBase16String(const std::string base_16) {
  std::string compressed;
  DCHECK_EQ(base_16.size() % 2, 0u) << "Must be a multiple of 2";
  compressed.reserve(base_16.size() / 2);

  std::vector<uint8> v;
  if (!base::HexStringToBytes(base_16, &v)) {
    NOTREACHED();
  }
  compressed.assign(reinterpret_cast<const char*>(&v[0]), v.size());
  return compressed;
}

}  // namespace cast
}  // namespace media
