// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_TRANSFER_CACHE_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_TRANSFER_CACHE_H_

#include "cc/paint/transfer_cache_entry.h"
#include "gpu/command_buffer/client/client_discardable_manager.h"
#include "gpu/command_buffer/client/gles2_impl_export.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/transfer_cache_entry_id.h"

namespace gpu {
namespace gles2 {
class GLES2CmdHelper;
}
class MappedMemoryManager;

// ClientTransferCache allows for ClientTransferCacheEntries to be inserted
// into the cache, which will send them to the ServiceTransferCache, making
// them available for consumption in the GPU process. Typical usage is:
//   1) Insert a new entry via CreateCacheEntry. It starts locked.
//   2) Use the new entry's ID in one or more commands.
//   3) Unlock the entry via UnlockTransferCacheEntry after dependent commands
//      have been issued.
//
// If an entry is needed again:
//   4) Attempt to lock the entry via LockTransferCacheEntry.
//      4a) If this fails, DeleteTransferCacheEntry then go to (1)
//      4b) If this succeeds, go to (2).
//
// If an entry is no longer needed:
//   5) DeleteTransferCacheEntry
//
class GLES2_IMPL_EXPORT ClientTransferCache {
 public:
  ClientTransferCache();
  ~ClientTransferCache();

  TransferCacheEntryId CreateCacheEntry(
      gles2::GLES2CmdHelper* helper,
      MappedMemoryManager* mapped_memory,
      const cc::ClientTransferCacheEntry& entry);
  bool LockTransferCacheEntry(TransferCacheEntryId id);
  void UnlockTransferCacheEntry(gles2::GLES2CmdHelper* helper,
                                TransferCacheEntryId id);
  void DeleteTransferCacheEntry(gles2::GLES2CmdHelper* helper,
                                TransferCacheEntryId id);

  // Test only functions
  ClientDiscardableManager* DiscardableManagerForTesting() {
    return &discardable_manager_;
  }

 private:
  ClientDiscardableManager discardable_manager_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_TRANSFER_CACHE_H_
