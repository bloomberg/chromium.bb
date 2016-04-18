// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_COMMON_BLOB_CACHE_ID_UTIL_H_
#define BLIMP_COMMON_BLOB_CACHE_ID_UTIL_H_

#include <string>
#include <vector>

#include "blimp/common/blimp_common_export.h"
#include "blimp/common/blob_cache/blob_cache.h"

namespace blimp {

// Returns the bytes of a SHA1 hash of the input. The string is guaranteed
// to have length base::kSHA1Length.
BLIMP_COMMON_EXPORT const BlobId CalculateBlobId(const void* data,
                                                 size_t data_size);

// Returns a hexadecimal string representation of a SHA1 hash. The input is
// required to have length base::kSHA1Length.
BLIMP_COMMON_EXPORT const std::string FormatBlobId(const BlobId& data);

}  // namespace blimp

#endif  // BLIMP_COMMON_BLOB_CACHE_ID_UTIL_H_
