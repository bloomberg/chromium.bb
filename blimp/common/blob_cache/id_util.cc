// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/blob_cache/id_util.h"

#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "blimp/common/blob_cache/blob_cache.h"

namespace blimp {

const BlobId CalculateBlobId(const void* data, size_t data_size) {
  std::vector<unsigned char> sha1_hash(base::kSHA1Length);
  base::SHA1HashBytes(static_cast<const unsigned char*>(data), data_size,
                      sha1_hash.data());
  return std::string(reinterpret_cast<char*>(sha1_hash.data()),
                     sha1_hash.size());
}

const std::string FormatBlobId(const BlobId& data) {
  DCHECK_EQ(base::kSHA1Length, data.length());
  return base::ToLowerASCII(base::HexEncode(data.data(), data.length()));
}

}  // namespace blimp
