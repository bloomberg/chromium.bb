// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_disk_format.h"

#include "base/hash.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/stringprintf.h"

namespace disk_cache {

std::string GetEntryHashForKey(const std::string& key) {
  const std::string sha_hash = base::SHA1HashString(key);
  const std::string key_hash = base::StringPrintf(
      "%02x%02x%02x%02x%02x",
      implicit_cast<unsigned char>(sha_hash[0]),
      implicit_cast<unsigned char>(sha_hash[1]),
      implicit_cast<unsigned char>(sha_hash[2]),
      implicit_cast<unsigned char>(sha_hash[3]),
      implicit_cast<unsigned char>(sha_hash[4]));
  DCHECK_EQ(static_cast<size_t>(kEntryHashKeySize), key_hash.size());
  return key_hash;
}

}  // namespace disk_cache
