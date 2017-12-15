// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/transfer_cache_serialize_helper.h"

#include <utility>

#include "base/logging.h"

namespace cc {

TransferCacheSerializeHelper::TransferCacheSerializeHelper() = default;
TransferCacheSerializeHelper::~TransferCacheSerializeHelper() = default;

bool TransferCacheSerializeHelper::LockEntry(TransferCacheEntryType type,
                                             uint32_t id) {
  // Entry already locked, so we don't need to process it.
  if (added_entries_.count(std::make_pair(type, id)) != 0)
    return true;

  bool success = LockEntryInternal(type, id);
  if (!success)
    return false;
  added_entries_.emplace(type, id);
  return true;
}

void TransferCacheSerializeHelper::CreateEntry(
    const ClientTransferCacheEntry& entry) {
  // We shouldn't be creating entries if they were already created or locked.
  DCHECK_EQ(added_entries_.count(std::make_pair(entry.Type(), entry.Id())), 0u);
  CreateEntryInternal(entry);
  added_entries_.emplace(entry.Type(), entry.Id());
}

void TransferCacheSerializeHelper::FlushEntries() {
  FlushEntriesInternal(
      std::vector<EntryKey>(added_entries_.begin(), added_entries_.end()));
  added_entries_.clear();
}

void TransferCacheSerializeHelper::AssertLocked(TransferCacheEntryType type,
                                                uint32_t id) {
  DCHECK_EQ(added_entries_.count(std::make_pair(type, id)), 1u);
}

}  // namespace cc
