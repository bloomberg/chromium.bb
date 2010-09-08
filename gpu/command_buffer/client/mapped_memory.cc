// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <functional>

#include "../client/mapped_memory.h"
#include "../client/cmd_buffer_helper.h"

namespace gpu {
namespace {
void DeleteMemoryChunk(MemoryChunk* chunk) {
  delete chunk;
}
}

MemoryChunk::MemoryChunk(
    int32 shm_id, gpu::Buffer shm, CommandBufferHelper* helper)
    : shm_id_(shm_id),
      shm_(shm),
      allocator_(shm.size, helper, shm.ptr) {
}

MappedMemoryManager::~MappedMemoryManager() {
  std::for_each(chunks_.begin(),
                chunks_.end(),
                std::pointer_to_unary_function<MemoryChunk*, void>(
                    DeleteMemoryChunk));
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
  int32 id = cmd_buf->CreateTransferBuffer(size);
  if (id == -1) {
    return NULL;
  }
  gpu::Buffer shm = cmd_buf->GetTransferBuffer(id);
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

}  // namespace gpu



