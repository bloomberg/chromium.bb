// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_transfer_cache.h"

#include "base/sys_info.h"
#include "cc/paint/raw_memory_transfer_cache_entry.h"

namespace gpu {
namespace {
size_t CacheSizeLimit() {
  if (base::SysInfo::IsLowEndDevice()) {
    return 4 * 1024 * 1024;
  } else {
    return 128 * 1024 * 1024;
  }
}

}  // namespace

ServiceTransferCache::CacheEntryInternal::CacheEntryInternal(
    ServiceDiscardableHandle handle,
    std::unique_ptr<cc::ServiceTransferCacheEntry> entry)
    : handle(handle), entry(std::move(entry)) {}

ServiceTransferCache::CacheEntryInternal::~CacheEntryInternal() = default;

ServiceTransferCache::CacheEntryInternal::CacheEntryInternal(
    CacheEntryInternal&& other) = default;

ServiceTransferCache::CacheEntryInternal&
ServiceTransferCache::CacheEntryInternal::operator=(
    CacheEntryInternal&& other) = default;

ServiceTransferCache::ServiceTransferCache()
    : entries_(EntryCache::NO_AUTO_EVICT),
      cache_size_limit_(CacheSizeLimit()) {}

ServiceTransferCache::~ServiceTransferCache() = default;

bool ServiceTransferCache::CreateLockedEntry(TransferCacheEntryId id,
                                             ServiceDiscardableHandle handle,
                                             cc::TransferCacheEntryType type,
                                             GrContext* context,
                                             base::span<uint8_t> data) {
  auto found = entries_.Peek(id);
  if (found != entries_.end()) {
    return false;
  }

  std::unique_ptr<cc::ServiceTransferCacheEntry> entry =
      cc::ServiceTransferCacheEntry::Create(type);
  if (!entry)
    return false;

  entry->Deserialize(context, data);
  total_size_ += entry->CachedSize();
  entries_.Put(id, CacheEntryInternal(handle, std::move(entry)));
  EnforceLimits();
  return true;
}

bool ServiceTransferCache::UnlockEntry(TransferCacheEntryId id) {
  auto found = entries_.Peek(id);
  if (found == entries_.end())
    return false;

  found->second.handle.Unlock();
  return true;
}

bool ServiceTransferCache::DeleteEntry(TransferCacheEntryId id) {
  auto found = entries_.Peek(id);
  if (found == entries_.end())
    return false;

  found->second.handle.ForceDelete();
  total_size_ -= found->second.entry->CachedSize();
  entries_.Erase(found);
  return true;
}

cc::ServiceTransferCacheEntry* ServiceTransferCache::GetEntry(
    TransferCacheEntryId id) {
  auto found = entries_.Get(id);
  if (found == entries_.end())
    return nullptr;
  return found->second.entry.get();
}

void ServiceTransferCache::EnforceLimits() {
  for (auto it = entries_.rbegin(); it != entries_.rend();) {
    if (total_size_ <= cache_size_limit_) {
      return;
    }
    if (!it->second.handle.Delete()) {
      ++it;
      continue;
    }

    total_size_ -= it->second.entry->CachedSize();
    it = entries_.Erase(it);
  }
}

}  // namespace gpu
