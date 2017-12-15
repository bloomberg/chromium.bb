// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_transfer_cache.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/mapped_memory.h"

namespace gpu {

ClientTransferCache::ClientTransferCache() = default;
ClientTransferCache::~ClientTransferCache() = default;

void ClientTransferCache::CreateCacheEntry(
    gles2::GLES2CmdHelper* helper,
    MappedMemoryManager* mapped_memory,
    const cc::ClientTransferCacheEntry& entry) {
  base::AutoLock hold(lock_);
  ScopedMappedMemoryPtr mapped_alloc(entry.SerializedSize(), helper,
                                     mapped_memory);
  DCHECK(mapped_alloc.valid());
  bool succeeded = entry.Serialize(base::make_span(
      reinterpret_cast<uint8_t*>(mapped_alloc.address()), mapped_alloc.size()));
  DCHECK(succeeded);

  ClientDiscardableHandle::Id id =
      discardable_manager_.CreateHandle(helper->command_buffer());
  ClientDiscardableHandle handle = discardable_manager_.GetHandle(id);

  // Store the mapping from the given namespace/id to the transfer cache id.
  DCHECK(FindDiscardableHandleId(entry.Type(), entry.Id()).is_null());
  DiscardableHandleIdMap(entry.Type())[entry.Id()] = id;

  helper->CreateTransferCacheEntryINTERNAL(
      static_cast<uint32_t>(entry.Type()), entry.Id(), handle.shm_id(),
      handle.byte_offset(), mapped_alloc.shm_id(), mapped_alloc.offset(),
      mapped_alloc.size());
}

bool ClientTransferCache::LockTransferCacheEntry(
    cc::TransferCacheEntryType type,
    uint32_t id) {
  base::AutoLock hold(lock_);
  auto discardable_handle_id = FindDiscardableHandleId(type, id);
  if (discardable_handle_id.is_null())
    return false;

  if (discardable_manager_.LockHandle(discardable_handle_id))
    return true;

  // Could not lock. Entry is already deleted service side.
  DiscardableHandleIdMap(type).erase(id);
  return false;
}

void ClientTransferCache::UnlockTransferCacheEntries(
    gles2::GLES2CmdHelper* helper,
    const std::vector<std::pair<cc::TransferCacheEntryType, uint32_t>>&
        entries) {
  base::AutoLock hold(lock_);
  for (const auto& entry : entries) {
    auto type = entry.first;
    auto id = entry.second;
    DCHECK(!FindDiscardableHandleId(type, id).is_null());
    helper->UnlockTransferCacheEntryINTERNAL(static_cast<uint32_t>(type), id);
  }
}

void ClientTransferCache::DeleteTransferCacheEntry(
    gles2::GLES2CmdHelper* helper,
    cc::TransferCacheEntryType type,
    uint32_t id) {
  base::AutoLock hold(lock_);
  auto discardable_handle_id = FindDiscardableHandleId(type, id);
  if (discardable_handle_id.is_null())
    return;

  discardable_manager_.FreeHandle(discardable_handle_id);
  helper->DeleteTransferCacheEntryINTERNAL(static_cast<uint32_t>(type), id);
  DiscardableHandleIdMap(type).erase(id);
}

ClientDiscardableHandle::Id ClientTransferCache::FindDiscardableHandleId(
    cc::TransferCacheEntryType type,
    uint32_t id) {
  lock_.AssertAcquired();
  const auto& id_map = DiscardableHandleIdMap(type);
  auto id_map_it = id_map.find(id);
  if (id_map_it == id_map.end())
    return ClientDiscardableHandle::Id();
  return id_map_it->second;
}

std::map<uint32_t, ClientDiscardableHandle::Id>&
ClientTransferCache::DiscardableHandleIdMap(
    cc::TransferCacheEntryType entry_type) {
  lock_.AssertAcquired();
  DCHECK_LE(static_cast<uint32_t>(entry_type),
            static_cast<uint32_t>(cc::TransferCacheEntryType::kLast));
  return discardable_handle_id_map_[static_cast<uint32_t>(entry_type)];
}

}  // namespace gpu
