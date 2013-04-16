// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_disk_format.h"

#include "base/format_macros.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"

namespace disk_cache {

SimpleFileHeader::SimpleFileHeader() {
  // Make hashing repeatable: leave no padding bytes untouched.
  memset(this, 0, sizeof(*this));
}

std::string ConvertEntryHashKeyToHexString(uint64 hash_key) {
  const std::string hash_key_str = base::StringPrintf("%016" PRIx64, hash_key);
  DCHECK_EQ(kEntryHashKeyAsHexStringSize, hash_key_str.size());
  return hash_key_str;
}

std::string GetEntryHashKeyAsHexString(const std::string& key) {
  std::string hash_key_str =
      ConvertEntryHashKeyToHexString(GetEntryHashKey(key));
  DCHECK_EQ(kEntryHashKeyAsHexStringSize, hash_key_str.size());
  return hash_key_str;
}

bool GetEntryHashKeyFromHexString(const std::string& hash_key,
                                  uint64* hash_key_out) {
  if (hash_key.size() != kEntryHashKeyAsHexStringSize) {
    return false;
  }
  return base::HexStringToUInt64(hash_key, hash_key_out);
}

uint64 GetEntryHashKey(const std::string& key) {
  const std::string sha_hash = base::SHA1HashString(key);
  uint64 hash_key = 0;
  sha_hash.copy(reinterpret_cast<char*>(&hash_key), sizeof(hash_key));
  return hash_key;
}

namespace SimpleIndexFile {

Footer::Footer() {
  // Make hashing repeatable: leave no padding bytes untouched.
  memset(this, 0, sizeof(*this));
}

Header::Header() {
  // Make hashing repeatable: leave no padding bytes untouched.
  memset(this, 0, sizeof(*this));
}

EntryMetadata::EntryMetadata() {
  // Make hashing repeatable: leave no padding bytes untouched.
  memset(this, 0, sizeof(*this));
}

EntryMetadata::EntryMetadata(uint64 hash_key_p,
                             base::Time last_used_time_p,
                             uint64 entry_size_p) {
  // Make hashing repeatable: leave no padding bytes untouched.
  memset(this, 0, sizeof(*this));

  // Proceed with field initializations.
  hash_key = hash_key_p;
  entry_size = entry_size_p;
  last_used_time = last_used_time_p.ToInternalValue();
}

uint64 EntryMetadata::GetHashKey() const {
  return hash_key;
}

base::Time EntryMetadata::GetLastUsedTime() const {
  return base::Time::FromInternalValue(last_used_time);
}

void EntryMetadata::SetLastUsedTime(const base::Time& last_used_time_p) {
  last_used_time = last_used_time_p.ToInternalValue();
}

// static
void EntryMetadata::Serialize(const EntryMetadata& in_entry_metadata,
                              std::string* out_buffer) {
  DCHECK(out_buffer);
  // TODO(felipeg): We may choose to, instead, serialize each struct member
  // separately.
  out_buffer->append(reinterpret_cast<const char*>(&in_entry_metadata),
                     kEntryMetadataSize);
}

// static
void EntryMetadata::DeSerialize(const char* in_buffer,
                                EntryMetadata* out_entry_metadata) {
  DCHECK(in_buffer);
  DCHECK(out_entry_metadata);
  memcpy(out_entry_metadata, in_buffer, kEntryMetadataSize);
}

// static
void EntryMetadata::Merge(const EntryMetadata& from,
                          EntryMetadata* to) {
  if (to->last_used_time == 0)
    to->last_used_time = from.last_used_time;
  if (to->entry_size == 0)
    to->entry_size = from.entry_size;
}

}  // namespace SimpleIndexFile

}  // namespace disk_cache
