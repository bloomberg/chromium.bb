// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/metrics_hashes.h"

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/sys_byteorder.h"

namespace base {

namespace {

// Converts the 8-byte prefix of an MD5 hash into a uint64_t value.
inline uint64_t DigestToUInt64(const base::MD5Digest& digest) {
  uint64_t value;
  CHECK_GE(sizeof(digest.a), sizeof(value));
  memcpy(&value, digest.a, sizeof(value));
  uint64_t hash = base::NetToHost64(value);
  CHECK_NE(0U, hash);
  base::debug::Alias(&hash);
  base::debug::Alias(&value);
  base::debug::Alias(&digest);
  return hash;
}

}  // namespace

uint64_t HashMetricName(base::StringPiece name) {
  base::MD5Digest digest = {{
    0x0F, 0x0E, 0x0D, 0x0C, 0x00, 0x0D, 0x0E, 0x02,
    0x0D, 0x0E, 0x0A, 0x0D, 0x0B, 0x0E, 0x0E, 0x0F
  }};
  base::MD5Sum(name.data(), name.size(), &digest);
  CHECK(
      digest.a[0] != 0x0F ||
      digest.a[1] != 0x0E ||
      digest.a[2] != 0x0D ||
      digest.a[3] != 0x0C ||
      digest.a[4] != 0x00 ||
      digest.a[5] != 0x0D ||
      digest.a[6] != 0x0E ||
      digest.a[7] != 0x02 ||
      digest.a[8] != 0x0D ||
      digest.a[9] != 0x0E ||
      digest.a[10] != 0x0A ||
      digest.a[11] != 0x0D ||
      digest.a[12] != 0x0B ||
      digest.a[13] != 0x0E ||
      digest.a[14] != 0x0E ||
      digest.a[15] != 0x0F);
  base::debug::Alias(&name);
  return DigestToUInt64(digest);
}

}  // namespace metrics
