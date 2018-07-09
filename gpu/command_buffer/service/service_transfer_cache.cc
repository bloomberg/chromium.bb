// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_transfer_cache.h"

#include <inttypes.h>

#include "base/bind.h"
#include "base/memory/memory_coordinator_client_registry.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/trace_event/memory_dump_manager.h"
#include "cc/paint/image_transfer_cache_entry.h"

namespace gpu {
namespace {
size_t CacheSizeLimit(base::MemoryState state) {
  size_t normal_state_memory_usage = 128 * 1024 * 1024;
  if (base::SysInfo::IsLowEndDevice())
    normal_state_memory_usage = 4 * 1024 * 1024;

  switch (state) {
    case base::MemoryState::NORMAL:
      return normal_state_memory_usage;
    case base::MemoryState::THROTTLED:
      return normal_state_memory_usage / 2;
    case base::MemoryState::SUSPENDED:
      return 0u;
    case base::MemoryState::UNKNOWN:
      NOTREACHED();
  }
  return normal_state_memory_usage;
}

}  // namespace

ServiceTransferCache::CacheEntryInternal::CacheEntryInternal(
    base::Optional<ServiceDiscardableHandle> handle,
    std::unique_ptr<cc::ServiceTransferCacheEntry> entry)
    : handle(handle), entry(std::move(entry)) {}

ServiceTransferCache::CacheEntryInternal::~CacheEntryInternal() {}

ServiceTransferCache::CacheEntryInternal::CacheEntryInternal(
    CacheEntryInternal&& other) = default;

ServiceTransferCache::CacheEntryInternal&
ServiceTransferCache::CacheEntryInternal::operator=(
    CacheEntryInternal&& other) = default;

ServiceTransferCache::ServiceTransferCache()
    : entries_(EntryCache::NO_AUTO_EVICT),
      cache_size_limit_(CacheSizeLimit(memory_state_)),
      memory_pressure_listener_(
          base::BindRepeating(&ServiceTransferCache::OnMemoryPressure,
                              base::Unretained(this))) {
  base::MemoryCoordinatorClientRegistry::GetInstance()->Register(this);

  // In certain cases, ThreadTaskRunnerHandle isn't set (Android Webview).
  // Don't register a dump provider in these cases.
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "cc::GpuImageDecodeCache", base::ThreadTaskRunnerHandle::Get());
  }
}

ServiceTransferCache::~ServiceTransferCache() {
  base::MemoryCoordinatorClientRegistry::GetInstance()->Unregister(this);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool ServiceTransferCache::CreateLockedEntry(
    cc::TransferCacheEntryType entry_type,
    uint32_t entry_id,
    ServiceDiscardableHandle handle,
    GrContext* context,
    base::span<uint8_t> data) {
  auto key = std::make_pair(entry_type, entry_id);
  auto found = entries_.Peek(key);
  if (found != entries_.end())
    return false;

  std::unique_ptr<cc::ServiceTransferCacheEntry> entry =
      cc::ServiceTransferCacheEntry::Create(entry_type);
  if (!entry)
    return false;

  if (!entry->Deserialize(context, data))
    return false;

  total_size_ += entry->CachedSize();
  entries_.Put(key, CacheEntryInternal(handle, std::move(entry)));
  EnforceLimits();
  return true;
}

void ServiceTransferCache::CreateLocalEntry(
    uint32_t entry_id,
    std::unique_ptr<cc::ServiceTransferCacheEntry> entry) {
  if (!entry)
    return;

  DeleteEntry(entry->Type(), entry_id);

  total_size_ += entry->CachedSize();

  auto key = std::make_pair(entry->Type(), entry_id);
  entries_.Put(key, CacheEntryInternal(base::nullopt, std::move(entry)));
  EnforceLimits();
}

bool ServiceTransferCache::UnlockEntry(cc::TransferCacheEntryType entry_type,
                                       uint32_t entry_id) {
  auto key = std::make_pair(entry_type, entry_id);
  auto found = entries_.Peek(key);
  if (found == entries_.end())
    return false;

  if (!found->second.handle)
    return false;
  found->second.handle->Unlock();
  return true;
}

bool ServiceTransferCache::DeleteEntry(cc::TransferCacheEntryType entry_type,
                                       uint32_t entry_id) {
  auto key = std::make_pair(entry_type, entry_id);
  auto found = entries_.Peek(key);
  if (found == entries_.end())
    return false;

  if (found->second.handle)
    found->second.handle->ForceDelete();
  total_size_ -= found->second.entry->CachedSize();
  entries_.Erase(found);
  return true;
}

cc::ServiceTransferCacheEntry* ServiceTransferCache::GetEntry(
    cc::TransferCacheEntryType entry_type,
    uint32_t entry_id) {
  auto key = std::make_pair(entry_type, entry_id);
  auto found = entries_.Get(key);
  if (found == entries_.end())
    return nullptr;
  return found->second.entry.get();
}

void ServiceTransferCache::EnforceLimits() {
  for (auto it = entries_.rbegin(); it != entries_.rend();) {
    if (total_size_ <= cache_size_limit_) {
      return;
    }
    if (it->second.handle && !it->second.handle->Delete()) {
      ++it;
      continue;
    }

    total_size_ -= it->second.entry->CachedSize();
    it = entries_.Erase(it);
  }
}

void ServiceTransferCache::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  if (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL)
    OnPurgeMemory();
}

void ServiceTransferCache::OnMemoryStateChange(base::MemoryState state) {
  memory_state_ = state;
  cache_size_limit_ = CacheSizeLimit(memory_state_);
}

void ServiceTransferCache::OnPurgeMemory() {
  cache_size_limit_ = 0u;
  EnforceLimits();
  cache_size_limit_ = CacheSizeLimit(memory_state_);
}

bool ServiceTransferCache::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  using base::trace_event::MemoryAllocatorDumpGuid;
  using base::trace_event::MemoryDumpLevelOfDetail;

  if (args.level_of_detail == MemoryDumpLevelOfDetail::BACKGROUND) {
    std::string dump_name =
        base::StringPrintf("gpu/transfer_cache/cache_0x%" PRIXPTR,
                           reinterpret_cast<uintptr_t>(this));
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, total_size_);

    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  for (auto it = entries_.begin(); it != entries_.end(); it++) {
    uint32_t entry_id = it->first.second;
    auto entry_type = it->first.first;
    const auto* entry = it->second.entry.get();

    std::string dump_name;
    if (entry_type == cc::TransferCacheEntryType::kImage &&
        static_cast<const cc::ServiceImageTransferCacheEntry*>(entry)
            ->fits_on_gpu()) {
      dump_name = base::StringPrintf(
          "gpu/transfer_cache/cache_0x%" PRIXPTR "/gpu/entry_%d",
          reinterpret_cast<uintptr_t>(this), entry_id);
    } else {
      dump_name = base::StringPrintf(
                "gpu/transfer_cache/cache_0x%" PRIXPTR "/cpu/entry_%d",
                reinterpret_cast<uintptr_t>(this), entry_id);
    }

    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, entry->CachedSize());
  }

  return true;
}

}  // namespace gpu
