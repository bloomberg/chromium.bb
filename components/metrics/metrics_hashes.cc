// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_hashes.h"

#include "base/logging.h"
#include "base/md5.h"
#include "base/sys_byteorder.h"

namespace metrics {

namespace {

// Converts the 8-byte prefix of an MD5 hash into a uint64 value.
inline uint64 DigestToUInt64(const base::MD5Digest& digest) {
  uint64 value;
  DCHECK_GE(arraysize(digest.a), sizeof(value));
  memcpy(&value, digest.a, sizeof(value));
  return base::HostToNet64(value);
}

}  // namespace

uint64 HashMetricName(const std::string& name) {
  base::MD5Digest digest;
  base::MD5Sum(name.c_str(), name.size(), &digest);
  return DigestToUInt64(digest);
}

}  // namespace metrics
