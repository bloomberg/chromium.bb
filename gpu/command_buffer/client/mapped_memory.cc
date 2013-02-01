// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <functional>

#include "../client/mapped_memory.h"
#include "../client/cmd_buffer_helper.h"

namespace gpu {

MemoryChunk::MemoryChunk(
    int32 shm_id, gpu::Buffer shm, CommandBufferHelper* helper)
    : shm_id_(shm_id),
      shm_(shm),
      allocator_(shm.size, helper, shm.ptr) {
}

MappedMemoryManager::MappedMemoryManager(CommandBufferHelper* helper)
    : chunk_size_multiple_(1),
      helper_(helper) {
}

MappedMemoryManager::~MappedMemoryManager() {
  CommandBuffer* cmd_buf = helper_->command_buffer();
  for (MemoryChunkVector::iterator iter = chunks_.begin();
       iter != chunks_.end(); ++iter) {
    MemoryChunk* chunk = *iter;
    cmd_buf->DestroyTransferBuffer(chunk->shm_id());
    delete chunk;
  }
}

void* MappedMemoryManager::Alloc(
    unsigned int size, int32* shm_id, unsigned int* shm_offset) {
  GPU_DCHECK(shm_id);
  GPU_DCHECK(shm_offset);
  // See if any of the chucks can satisfy this request.
  for (size_t ii = 0; ii < chunks_.size(); ++ii) {
    MemoryChunk* chunk = chunks_[ii];
    chunk->FreeUnused();
    if (chunk->GetLargestFreeSizeWithoutWaiting() >= size) {
      void* mem = chunk->Alloc(size);
      GPU_DCHECK(mem);
      *shm_id = chunk->shm_id();
      *shm_offset = chunk->GetOffset(mem);
      return mem;
    }
  }

  // Make a new chunk to satisfy the request.
  CommandBuffer* cmd_buf = helper_->command_buffer();
  unsigned int chunk_size =
      ((size + chunk_size_multiple_ - 1) / chunk_size_multiple_) *
      chunk_size_multiple_;
  int32 id = -1;
  gpu::Buffer shm = cmd_buf->CreateTransferBuffer(chunk_size, &id);
  if (id  < 0)
    return NULL;
  MemoryChunk* mc = new MemoryChunk(id, shm, helper_);
  chunks_.push_back(mc);
  void* mem = mc->Alloc(size);
  GPU_DCHECK(mem);
  *shm_id = mc->shm_id();
  *shm_offset = mc->GetOffset(mem);
  return mem;
}

void MappedMemoryManager::Free(void* pointer) {
  for (size_t ii = 0; ii < chunks_.size(); ++ii) {
    MemoryChunk* chunk = chunks_[ii];
    if (chunk->IsInChunk(pointer)) {
      chunk->Free(pointer);
      return;
    }
  }
  GPU_NOTREACHED();
}

void MappedMemoryManager::FreePendingToken(void* pointer, int32 token) {
  for (size_t ii = 0; ii < chunks_.size(); ++ii) {
    MemoryChunk* chunk = chunks_[ii];
    if (chunk->IsInChunk(pointer)) {
      chunk->FreePendingToken(pointer, token);
      return;
    }
  }
  GPU_NOTREACHED();
}

void MappedMemoryManager::FreeUnused() {
  CommandBuffer* cmd_buf = helper_->command_buffer();
  MemoryChunkVector::iterator iter = chunks_.begin();
  while (iter != chunks_.end()) {
    MemoryChunk* chunk = *iter;
    chunk->FreeUnused();
    if (!chunk->InUse()) {
      cmd_buf->DestroyTransferBuffer(chunk->shm_id());
      iter = chunks_.erase(iter);
    } else {
      ++iter;
    }
  }
}

}  // namespace gpu



