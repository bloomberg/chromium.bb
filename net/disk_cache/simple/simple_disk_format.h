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

// Simple Index File sketch:
// This is based on the struct Header and Footer as seem below, and the struct
// alignment is platform dependent.
// The CRC check is a guarantee that we don't read incorrect values.
// -------------------------
//    uint64 initial_magic_number
//    uint32 version
//    uint64 size
// -------------------------
//    Repeated |size| times {
//       bytes[kEntryHashKeySize]
//    }
// -------------------------
//    uint32 crc
// -------------------------
namespace SimpleIndexFile {
  // Simple Index File metadata is defined here.
  struct Header {
    uint64 initial_magic_number;
    uint32 version;
    uint64 size;
  };

  struct Footer {
    uint32 crc;
  };

}  // namespace SimpleIndexFile

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_DISK_FORMAT_H_
