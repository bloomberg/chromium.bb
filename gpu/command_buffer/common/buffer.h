// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_BUFFER_H_
#define GPU_COMMAND_BUFFER_COMMON_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "gpu/gpu_export.h"

namespace base {

class SharedMemory;
class SharedMemoryHandle;

}  // namespace base

namespace gpu {

class GPU_EXPORT BufferBacking {
 public:
  virtual ~BufferBacking() {}
  virtual base::SharedMemoryHandle shared_memory_handle() const;
  virtual void* GetMemory() const = 0;
  virtual size_t GetSize() const = 0;
};

class GPU_EXPORT SharedMemoryBufferBacking : public BufferBacking {
 public:
  SharedMemoryBufferBacking(std::unique_ptr<base::SharedMemory> shared_memory,
                            size_t size);
  ~SharedMemoryBufferBacking() override;
  base::SharedMemoryHandle shared_memory_handle() const override;
  void* GetMemory() const override;
  size_t GetSize() const override;
  base::SharedMemory* shared_memory() { return shared_memory_.get(); }

 private:
  std::unique_ptr<base::SharedMemory> shared_memory_;
  size_t size_;
  DISALLOW_COPY_AND_ASSIGN(SharedMemoryBufferBacking);
};

// Buffer owns a piece of shared-memory of a certain size.
class GPU_EXPORT Buffer : public base::RefCountedThreadSafe<Buffer> {
 public:
  explicit Buffer(std::unique_ptr<BufferBacking> backing);

  BufferBacking* backing() const { return backing_.get(); }
  void* memory() const { return memory_; }
  size_t size() const { return size_; }

  // Returns NULL if the address overflows the memory.
  void* GetDataAddress(uint32_t data_offset, uint32_t data_size) const;

  // Returns NULL if the address overflows the memory.
  void* GetDataAddressAndSize(uint32_t data_offset, uint32_t* data_size) const;

  // Returns the remaining size of the buffer after an offset
  uint32_t GetRemainingSize(uint32_t data_offset) const;

 private:
  friend class base::RefCountedThreadSafe<Buffer>;
  ~Buffer();

  std::unique_ptr<BufferBacking> backing_;
  void* memory_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

static inline std::unique_ptr<BufferBacking> MakeBackingFromSharedMemory(
    std::unique_ptr<base::SharedMemory> shared_memory,
    size_t size) {
  return std::unique_ptr<BufferBacking>(
      new SharedMemoryBufferBacking(std::move(shared_memory), size));
}

static inline scoped_refptr<Buffer> MakeBufferFromSharedMemory(
    std::unique_ptr<base::SharedMemory> shared_memory,
    size_t size) {
  return new Buffer(
      MakeBackingFromSharedMemory(std::move(shared_memory), size));
}

// Generates GUID which can be used to trace buffer using an Id.
GPU_EXPORT base::trace_event::MemoryAllocatorDumpGuid GetBufferGUIDForTracing(
    uint64_t tracing_process_id,
    int32_t buffer_id);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_BUFFER_H_
