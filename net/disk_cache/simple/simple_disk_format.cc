// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_disk_format.h"

#include "base/hash.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/stringprintf.h"
#include "base/time.h"

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

namespace SimpleIndexFile {

EntryMetadata::EntryMetadata() :
    last_used_time(0),
    entry_size(0) {
}

EntryMetadata::EntryMetadata(const std::string& hash_key_p,
                             base::Time last_used_time_p,
                             uint64 entry_size_p) :
    last_used_time(last_used_time_p.ToInternalValue()),
    entry_size(entry_size_p) {
  DCHECK_EQ(kEntryHashKeySize, implicit_cast<int>(hash_key_p.size()));
  hash_key_p.copy(hash_key, kEntryHashKeySize);
}

std::string EntryMetadata::GetHashKey() const {
  return std::string(hash_key, kEntryHashKeySize);
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
