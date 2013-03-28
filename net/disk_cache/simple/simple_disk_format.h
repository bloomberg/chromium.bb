// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_DISK_FORMAT_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_DISK_FORMAT_H_

#include <string>

#include "base/basictypes.h"
#include "base/port.h"

namespace disk_cache {

const uint64 kSimpleInitialMagicNumber = GG_UINT64_C(0xfcfb6d1ba7725c30);

// A file in the Simple cache consists of a SimpleFileHeader followed
// by data.

const uint32 kSimpleVersion = 1;

static const int kSimpleEntryFileCount = 3;

struct SimpleFileHeader {
  uint64 initial_magic_number;
  uint32 version;
  uint32 key_length;
  uint32 key_hash;
};

// kHashKeySize must conform to the pattern in the GetHashForKey function.
const int kEntryHashKeySize = 10;
std::string GetEntryHashForKey(const std::string& key);

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_DISK_FORMAT_H_
