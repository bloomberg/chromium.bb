// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/transfer_cache_test_helper.h"

#include "base/containers/span.h"
#include "base/logging.h"

namespace cc {

TransferCacheTestHelper::TransferCacheTestHelper(GrContext* context)
    : context_(context) {}
TransferCacheTestHelper::~TransferCacheTestHelper() = default;

ServiceTransferCacheEntry* TransferCacheTestHelper::GetEntryInternal(
    TransferCacheEntryType type,
    uint32_t id) {
  auto key = std::make_pair(type, id);
  if (locked_entries_.count(key) == 0)
    return nullptr;
  DCHECK(entries_.find(key) != entries_.end());
  return entries_[key].get();
}

bool TransferCacheTestHelper::LockEntryInternal(TransferCacheEntryType type,
                                                uint32_t id) {
  auto key = std::make_pair(type, id);
  if (entries_.find(key) == entries_.end())
    return false;
  locked_entries_.insert(key);
  return true;
}

void TransferCacheTestHelper::CreateEntryInternal(
    const ClientTransferCacheEntry& client_entry) {
  auto key = std::make_pair(client_entry.Type(), client_entry.Id());
  DCHECK(entries_.find(key) == entries_.end());

  // Serialize data.
  size_t size = client_entry.SerializedSize();
  std::unique_ptr<uint8_t[]> data(new uint8_t[size]);
  bool success = client_entry.Serialize(base::make_span(data.get(), size));
  DCHECK(success);

  // Deserialize into a service transfer cache entry.
  std::unique_ptr<ServiceTransferCacheEntry> service_entry =
      ServiceTransferCacheEntry::Create(client_entry.Type());
  DCHECK(service_entry);
  success =
      service_entry->Deserialize(context_, base::make_span(data.get(), size));
  DCHECK(success);

  // Put things into the cache.
  entries_[key] = std::move(service_entry);
  locked_entries_.insert(key);
}

void TransferCacheTestHelper::FlushEntriesInternal(
    const std::set<EntryKey>& entries) {
  for (auto& entry : entries)
    locked_entries_.erase(entry);
}

}  // namespace cc
