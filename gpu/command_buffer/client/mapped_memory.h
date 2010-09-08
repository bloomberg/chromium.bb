// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_MAPPED_MEMORY_H_
#define GPU_COMMAND_BUFFER_CLIENT_MAPPED_MEMORY_H_

#include <vector>

#include "../common/types.h"
#include "../client/fenced_allocator.h"
#include "../common/buffer.h"

namespace gpu {

class CommandBufferHelper;

// Manages a shared memory segment.
class MemoryChunk {
 public:
  MemoryChunk(int32 shm_id, gpu::Buffer shm, CommandBufferHelper* helper);

  // Gets the size of the largest free block that is available without waiting.
  unsigned int GetLargestFreeSizeWithoutWaiting() {
    return allocator_.GetLargestFreeSize();
  }

  // Gets the size of the largest free block that can be allocated if the
  // caller can wait.
  unsigned int GetLargestFreeSizeWithWaiting() {
    return allocator_.GetLargestFreeOrPendingSize();
  }

  // Gets the size of the chunk.
  unsigned int GetSize() const {
    return shm_.size;
  }

  // The shared memory id for this chunk.
  int32 shm_id() const {
    return shm_id_;
  }

  // Allocates a block of memory. If the buffer is out of directly available
  // memory, this function may wait until memory that was freed "pending a
  // token" can be re-used.
  //
  // Parameters:
  //   size: the size of the memory block to allocate.
  //
  // Returns:
  //   the pointer to the allocated memory block, or NULL if out of
  //   memory.
  void* Alloc(unsigned int size) {
    return allocator_.Alloc(size);
  }

  // Gets the offset to a memory block given the base memory and the address.
  // It translates NULL to FencedAllocator::kInvalidOffset.
  unsigned int GetOffset(void* pointer) {
    return allocator_.GetOffset(pointer);
  }

  // Frees a block of memory.
  //
  // Parameters:
  //   pointer: the pointer to the memory block to free.
  void Free(void* pointer) {
    allocator_.Free(pointer);
  }

  // Frees a block of memory, pending the passage of a token. That memory won't
  // be re-allocated until the token has passed through the command stream.
  //
  // Parameters:
  //   pointer: the pointer to the memory block to free.
  //   token: the token value to wait for before re-using the memory.
  void FreePendingToken(void* pointer, unsigned int token) {
    allocator_.FreePendingToken(pointer, token);
  }

  // Frees any blocks who's tokens have passed.
  void FreeUnused() {
    allocator_.FreeUnused();
  }

  // Returns true if pointer is in the range of this block.
  bool IsInChunk(void* pointer) const {
    return pointer >= shm_.ptr &&
           pointer < reinterpret_cast<const int8*>(shm_.ptr) + shm_.size;
  }

 private:
  int32 shm_id_;
  gpu::Buffer shm_;
  FencedAllocatorWrapper allocator_;

  DISALLOW_COPY_AND_ASSIGN(MemoryChunk);
};

// Manages MemoryChucks.
class MappedMemoryManager {
 public:
  explicit MappedMemoryManager(CommandBufferHelper* helper)
      : helper_(helper) {
  }

  ~MappedMemoryManager();

  // Allocates a block of memory
  // Parameters:
  //   size: size of memory to allocate.
  //   shm_id: pointer to variable to receive the shared memory id.
  //   shm_offset: pointer to variable to receive the shared memory offset.
  // Returns:
  //   pointer to allocated block of memory. NULL if failure.
  void* Alloc(
      unsigned int size, int32* shm_id, unsigned int* shm_offset);

  // Frees a block of memory.
  //
  // Parameters:
  //   pointer: the pointer to the memory block to free.
  void Free(void* pointer);

  // Frees a block of memory, pending the passage of a token. That memory won't
  // be re-allocated until the token has passed through the command stream.
  //
  // Parameters:
  //   pointer: the pointer to the memory block to free.
  //   token: the token value to wait for before re-using the memory.
  void FreePendingToken(void* pointer, int32 token);

 private:
  typedef std::vector<MemoryChunk*> MemoryChunkVector;

  CommandBufferHelper* helper_;
  MemoryChunkVector chunks_;

  DISALLOW_COPY_AND_ASSIGN(MappedMemoryManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_MAPPED_MEMORY_H_

