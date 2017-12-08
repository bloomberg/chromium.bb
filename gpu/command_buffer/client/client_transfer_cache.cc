// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_transfer_cache.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/mapped_memory.h"

namespace gpu {

ClientTransferCache::ClientTransferCache() = default;
ClientTransferCache::~ClientTransferCache() = default;

TransferCacheEntryId ClientTransferCache::CreateCacheEntry(
    gles2::GLES2CmdHelper* helper,
    MappedMemoryManager* mapped_memory,
    const cc::ClientTransferCacheEntry& entry) {
  ScopedMappedMemoryPtr mapped_alloc(entry.SerializedSize(), helper,
                                     mapped_memory);
  DCHECK(mapped_alloc.valid());
  bool succeeded = entry.Serialize(base::make_span(
      reinterpret_cast<uint8_t*>(mapped_alloc.address()), mapped_alloc.size()));
  DCHECK(succeeded);

  TransferCacheEntryId id =
      discardable_manager_.CreateHandle(helper->command_buffer());
  ClientDiscardableHandle handle = discardable_manager_.GetHandle(id);

  helper->CreateTransferCacheEntryINTERNAL(
      id.GetUnsafeValue(), handle.shm_id(), handle.byte_offset(),
      static_cast<uint32_t>(entry.Type()), mapped_alloc.shm_id(),
      mapped_alloc.offset(), mapped_alloc.size());

  return id;
}

bool ClientTransferCache::LockTransferCacheEntry(TransferCacheEntryId id) {
  if (discardable_manager_.LockHandle(id))
    return true;

  // Could not lock. Entry is already deleted service side.
  return false;
}

void ClientTransferCache::UnlockTransferCacheEntry(
    gles2::GLES2CmdHelper* helper,
    TransferCacheEntryId id) {
  helper->UnlockTransferCacheEntryINTERNAL(id.GetUnsafeValue());
}

void ClientTransferCache::DeleteTransferCacheEntry(
    gles2::GLES2CmdHelper* helper,
    TransferCacheEntryId id) {
  discardable_manager_.FreeHandle(id);
  helper->DeleteTransferCacheEntryINTERNAL(id.GetUnsafeValue());
}

}  // namespace gpu
