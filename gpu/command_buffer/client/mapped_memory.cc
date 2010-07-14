// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/mapped_memory.h"
#include "gpu/command_buffer/client/cmd_buffer_helper.h"

namespace gpu {

MemoryChunk::MemoryChunk(
    int32 shm_id, gpu::Buffer shm, CommandBufferHelper* helper)
    : shm_id_(shm_id),
      shm_(shm),
      allocator_(shm.size, helper, shm.ptr) {
}

void* MappedMemoryManager::Alloc(
    unsigned int size, int32* shm_id, unsigned int* shm_offset) {
  DCHECK(shm_id);
  DCHECK(shm_offset);
  // See if any of the chucks can satisfy this request.
  for (size_t ii = 0; ii < chunks_.size(); ++ii) {
    MemoryChunk* chunk = chunks_[ii].get();
    chunk->FreeUnused();
    if (chunk->GetLargestFreeSizeWithoutWaiting() >= size) {
      void* mem = chunk->Alloc(size);
      DCHECK(mem);
      *shm_id = chunk->shm_id();
      *shm_offset = chunk->GetOffset(mem);
      return mem;
    }
  }

  // Make a new chunk to satisfy the request.
  CommandBuffer* cmd_buf = helper_->command_buffer();
  int32 id = cmd_buf->CreateTransferBuffer(size);
  if (id == -1) {
    return NULL;
  }
  gpu::Buffer shm = cmd_buf->GetTransferBuffer(id);
  MemoryChunk::Ref mc(new MemoryChunk(id, shm, helper_));
  chunks_.push_back(mc);
  void* mem = mc->Alloc(size);
  DCHECK(mem);
  *shm_id = mc->shm_id();
  *shm_offset = mc->GetOffset(mem);
  return mem;
}

void MappedMemoryManager::Free(void* pointer) {
  for (size_t ii = 0; ii < chunks_.size(); ++ii) {
    MemoryChunk* chunk = chunks_[ii].get();
    if (chunk->IsInChunk(pointer)) {
      chunk->Free(pointer);
      return;
    }
  }
  NOTREACHED();
}

void MappedMemoryManager::FreePendingToken(void* pointer, int32 token) {
  for (size_t ii = 0; ii < chunks_.size(); ++ii) {
    MemoryChunk* chunk = chunks_[ii].get();
    if (chunk->IsInChunk(pointer)) {
      chunk->FreePendingToken(pointer, token);
      return;
    }
  }
  NOTREACHED();
}

}  // namespace gpu



