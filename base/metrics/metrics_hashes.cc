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
  base::MD5Digest digest;
  base::MD5Sum(name.data(), name.size(), &digest);
  return DigestToUInt64(digest);
}

}  // namespace metrics
