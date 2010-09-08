// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the definition of the RingBuffer class.

#ifndef GPU_COMMAND_BUFFER_CLIENT_RING_BUFFER_H_
#define GPU_COMMAND_BUFFER_CLIENT_RING_BUFFER_H_

#include <deque>
#include "../common/logging.h"
#include "../common/types.h"

namespace gpu {
class CommandBufferHelper;

// RingBuffer manages a piece of memory as a ring buffer. Memory is allocated
// with Alloc and then a is freed pending a token with FreePendingToken.  Old
// allocations must not be kept past new allocations.
class RingBuffer {
 public:
  typedef unsigned int Offset;

  // Creates a RingBuffer.
  // Parameters:
  //   base_offset: The offset of the start of the buffer.
  //   size: The size of the buffer in bytes.
  //   helper: A CommandBufferHelper for dealing with tokens.
  RingBuffer(
      Offset base_offset, unsigned int size, CommandBufferHelper* helper);

  ~RingBuffer();

  // Allocates a block of memory. If the buffer is out of directly available
  // memory, this function may wait until memory that was freed "pending a
  // token" can be re-used.
  //
  // Parameters:
  //   size: the size of the memory block to allocate.
  //
  // Returns:
  //   the offset of the allocated memory block.
  Offset Alloc(unsigned int size);

  // Frees a block of memory, pending the passage of a token. That memory won't
  // be re-allocated until the token has passed through the command stream.
  //
  // Parameters:
  //   offset: the offset of the memory block to free.
  //   token: the token value to wait for before re-using the memory.
  void FreePendingToken(Offset offset, unsigned int token);

  // Gets the size of the largest free block that is available without waiting.
  unsigned int GetLargestFreeSizeNoWaiting();

  // Gets the size of the largest free block that can be allocated if the
  // caller can wait. Allocating a block of this size will succeed, but may
  // block.
  unsigned int GetLargestFreeOrPendingSize() {
    return size_;
  }

 private:
  // Book-keeping sturcture that describes a block of memory.
  struct Block {
    Block(Offset _offset, unsigned int _size)
        : offset(_offset),
          size(_size),
          token(0),
          valid(false) {
    }
    Offset offset;
    unsigned int size;
    unsigned int token;  // token to wait for.
    bool valid;  // whether or not token has been set.
  };

  typedef std::deque<Block> Container;
  typedef unsigned int BlockIndex;

  void FreeOldestBlock();

  CommandBufferHelper* helper_;

  // Used blocks are added to the end, blocks are freed from the beginning.
  Container blocks_;

  // The base offset of the ring buffer.
  Offset base_offset_;

  // The size of the ring buffer.
  Offset size_;

  // Offset of first free byte.
  Offset free_offset_;

  // Offset of first used byte.
  // Range between in_use_mark and free_mark is in use.
  Offset in_use_offset_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RingBuffer);
};

// This class functions just like RingBuffer, but its API uses pointers
// instead of offsets.
class RingBufferWrapper {
 public:
  // Parameters:
  //   base_offset: The offset to the start of the buffer
  //   size: The size of the buffer in bytes.
  //   helper: A CommandBufferHelper for dealing with tokens.
  //   base: The physical address that corresponds to base_offset.
  RingBufferWrapper(RingBuffer::Offset base_offset,
                    unsigned int size,
                    CommandBufferHelper* helper,
                    void* base)
      : allocator_(base_offset, size, helper),
        base_(static_cast<int8*>(base) - base_offset) {
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
    RingBuffer::Offset offset = allocator_.Alloc(size);
    return GetPointer(offset);
  }

  // Allocates a block of memory. If the buffer is out of directly available
  // memory, this function may wait until memory that was freed "pending a
  // token" can be re-used.
  // This is a type-safe version of Alloc, returning a typed pointer.
  //
  // Parameters:
  //   count: the number of elements to allocate.
  //
  // Returns:
  //   the pointer to the allocated memory block, or NULL if out of
  //   memory.
  template <typename T> T* AllocTyped(unsigned int count) {
    return static_cast<T*>(Alloc(count * sizeof(T)));
  }

  // Frees a block of memory, pending the passage of a token. That memory won't
  // be re-allocated until the token has passed through the command stream.
  //
  // Parameters:
  //   pointer: the pointer to the memory block to free.
  //   token: the token value to wait for before re-using the memory.
  void FreePendingToken(void* pointer, unsigned int token) {
    GPU_DCHECK(pointer);
    allocator_.FreePendingToken(GetOffset(pointer), token);
  }

  // Gets a pointer to a memory block given the base memory and the offset.
  void* GetPointer(RingBuffer::Offset offset) {
    return static_cast<int8*>(base_) + offset;
  }

  // Gets the offset to a memory block given the base memory and the address.
  RingBuffer::Offset GetOffset(void* pointer) {
    return static_cast<int8*>(pointer) - static_cast<int8*>(base_);
  }

  // Gets the size of the largest free block that is available without waiting.
  unsigned int GetLargestFreeSizeNoWaiting() {
    return allocator_.GetLargestFreeSizeNoWaiting();
  }

  // Gets the size of the largest free block that can be allocated if the
  // caller can wait.
  unsigned int GetLargestFreeOrPendingSize() {
    return allocator_.GetLargestFreeOrPendingSize();
  }

 private:
  RingBuffer allocator_;
  void* base_;
  RingBuffer::Offset base_offset_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(RingBufferWrapper);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_RING_BUFFER_H_
