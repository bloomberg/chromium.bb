// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_DISK_FORMAT_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_DISK_FORMAT_H_

#include <string>

#include "base/basictypes.h"
#include "base/port.h"
#include "net/base/net_export.h"

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

struct NET_EXPORT_PRIVATE SimpleFileHeader {
  SimpleFileHeader();
  uint64 initial_magic_number;
  uint32 version;
  uint32 key_length;
  uint32 key_hash;
};

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

  // TODO(felipeg): At some point we should consider using a protobuffer.  See
  // that we are re-implementing most of protobuffer's functionality such as
  // Serialize and Merge.
  // We must keep this struct a POD.
  struct EntryMetadata {
    EntryMetadata();
    EntryMetadata(uint64 hash_key_p,
                  base::Time last_used_time_p,
                  uint64 entry_size_p);

    base::Time GetLastUsedTime() const;
    uint64 GetHashKey() const;
    void SetLastUsedTime(const base::Time& last_used_time_p);

    // Serialize the data from |in_entry_metadata| and appends the bytes in
    // |out_buffer|. The serialization is platform dependent since it simply
    // writes the whole struct from memory into the given buffer.
    static void Serialize(const EntryMetadata& in_entry_metadata,
                          std::string* out_buffer);

    static void DeSerialize(const char* in_buffer,
                            EntryMetadata* out_entry_metadata);

    // Merge two EntryMetadata instances.
    // The existing valid data in |out_entry_metadata| will prevail.
    static void Merge(const EntryMetadata& entry_metadata,
                      EntryMetadata* out_entry_metadata);

    uint64 hash_key;

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

// Size of the uint64 hash_key number in Hex format in a string.
const size_t kEntryHashKeyAsHexStringSize = 2 * sizeof(uint64);

std::string ConvertEntryHashKeyToHexString(uint64 hash_key);

// |key| is the regular HTTP Cache key, which is a URL.
// Returns the Hex ascii representation of the uint64 hash_key.
std::string GetEntryHashKeyAsHexString(const std::string& key);

// |key| is the regular HTTP Cache key, which is a URL.
// Returns the hash of the key as uint64.
uint64 GetEntryHashKey(const std::string& key);

// Parses the |hash_key| string into a uint64 buffer.
// |hash_key| string must be of the form: FFFFFFFFFFFFFFFF .
bool GetEntryHashKeyFromHexString(const std::string& hash_key,
                                  uint64* hash_key_out);

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_DISK_FORMAT_H_
