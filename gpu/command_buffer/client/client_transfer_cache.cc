// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_transfer_cache.h"

namespace gpu {

ClientTransferCache::ClientTransferCache(Client* client) : client_(client) {}

ClientTransferCache::~ClientTransferCache() = default;

void* ClientTransferCache::MapEntry(MappedMemoryManager* mapped_memory,
                                    size_t size) {
  DCHECK(!mapped_ptr_);
  mapped_ptr_.emplace(size, client_->cmd_buffer_helper(), mapped_memory);
  if (!mapped_ptr_->valid()) {
    mapped_ptr_ = base::nullopt;
    return nullptr;
  } else {
    return mapped_ptr_->address();
  }
}

void ClientTransferCache::UnmapAndCreateEntry(uint32_t type, uint32_t id) {
  DCHECK(mapped_ptr_);
  EntryKey key(type, id);

  base::AutoLock hold(lock_);
  ClientDiscardableHandle::Id discardable_handle_id =
      discardable_manager_.CreateHandle(client_->command_buffer());
  if (discardable_handle_id.is_null())
    return;

  ClientDiscardableHandle handle =
      discardable_manager_.GetHandle(discardable_handle_id);

  // Store the mapping from the given namespace/discardable_handle_id to the
  // transfer cache discardable_handle_id.
  DCHECK(FindDiscardableHandleId(key).is_null());
  discardable_handle_id_map_.emplace(key, discardable_handle_id);

  client_->IssueCreateTransferCacheEntry(
      type, id, handle.shm_id(), handle.byte_offset(), mapped_ptr_->shm_id(),
      mapped_ptr_->offset(), mapped_ptr_->size());
  mapped_ptr_ = base::nullopt;
}

bool ClientTransferCache::LockEntry(uint32_t type, uint32_t id) {
  EntryKey key(type, id);

  base::AutoLock hold(lock_);
  auto discardable_handle_id = FindDiscardableHandleId(key);
  if (discardable_handle_id.is_null())
    return false;

  if (discardable_manager_.LockHandle(discardable_handle_id))
    return true;

  // Could not lock. Entry is already deleted service side.
  discardable_handle_id_map_.erase(key);
  return false;
}

void ClientTransferCache::UnlockEntries(
    const std::vector<std::pair<uint32_t, uint32_t>>& entries) {
  base::AutoLock hold(lock_);
  for (const auto& entry : entries) {
    DCHECK(!FindDiscardableHandleId(entry).is_null());
    client_->IssueUnlockTransferCacheEntry(entry.first, entry.second);
  }
}

void ClientTransferCache::DeleteEntry(uint32_t type, uint32_t id) {
  EntryKey key(type, id);
  base::AutoLock hold(lock_);
  auto discardable_handle_id = FindDiscardableHandleId(key);
  if (discardable_handle_id.is_null())
    return;

  discardable_manager_.FreeHandle(discardable_handle_id);
  client_->IssueDeleteTransferCacheEntry(type, id);
  discardable_handle_id_map_.erase(key);
}

ClientDiscardableHandle::Id ClientTransferCache::FindDiscardableHandleId(
    const EntryKey& key) {
  lock_.AssertAcquired();
  auto id_map_it = discardable_handle_id_map_.find(key);
  if (id_map_it == discardable_handle_id_map_.end())
    return ClientDiscardableHandle::Id();
  return id_map_it->second;
}

}  // namespace gpu
