// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_DISK_FORMAT_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_DISK_FORMAT_H_

#include <string>

#include "base/basictypes.h"
#include "base/port.h"

namespace base {
class Time;
}

namespace disk_cache {

const uint64 kSimpleInitialMagicNumber = GG_UINT64_C(0xfcfb6d1ba7725c30);
const uint64 kSimpleIndexInitialMagicNumber = GG_UINT64_C(0x656e74657220796f);

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
//    struct Header;
// -------------------------
//    Repeated |size| times {
//       struct EntryMetadata;
//    }
// -------------------------
//    struct Footer;
// -------------------------
namespace SimpleIndexFile {
  // Simple Index File metadata is defined here.
  struct Header {
    Header();
    uint64 initial_magic_number;
    uint32 version;
    uint64 number_of_entries;
    uint64 cache_size;  // Total cache storage size in bytes.
  };

  // We must keep this struct a POD.
  struct EntryMetadata {
    EntryMetadata();
    EntryMetadata(const std::string& hash_key_p,
                  base::Time last_used_time_p,
                  uint64 entry_size_p);

    base::Time GetLastUsedTime() const;
    std::string GetHashKey() const;
    void SetLastUsedTime(const base::Time& last_used_time_p);

    static void Serialize(const EntryMetadata& in_entry_metadata,
                          std::string* out_buffer);

    static void DeSerialize(const char* in_buffer,
                            EntryMetadata* out_entry_metadata);

    char hash_key[kEntryHashKeySize];  // Not a c_string, not null terminated.

    // This is the serialized format from Time::ToInternalValue().
    // If you want to make calculations/comparisons, you should use the
    // base::Time() class. Use the GetLastUsedTime() method above.
    int64 last_used_time;

    uint64 entry_size;  // Storage size in bytes.
  };

  const size_t kEntryMetadataSize = sizeof(EntryMetadata);

  struct Footer {
    Footer();
    uint32 crc;
  };

}  // namespace SimpleIndexFile

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_DISK_FORMAT_H_
