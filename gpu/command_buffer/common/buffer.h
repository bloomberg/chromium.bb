// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_BUFFER_H_
#define GPU_COMMAND_BUFFER_COMMON_BUFFER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "gpu/command_buffer/common/types.h"
#include "gpu/gpu_export.h"

namespace base {
  class SharedMemory;
}

namespace gpu {

// Buffer owns a piece of shared-memory of a certain size.
class GPU_EXPORT Buffer : public base::RefCountedThreadSafe<Buffer> {
 public:
  Buffer(scoped_ptr<base::SharedMemory> shared_memory, size_t size);

  base::SharedMemory* shared_memory() const { return shared_memory_.get(); }
  void* memory() const { return memory_; }
  size_t size() const { return size_; }

  // Returns NULL if the address overflows the memory.
  void* GetDataAddress(uint32 data_offset, uint32 data_size) const;

 private:
  friend class base::RefCountedThreadSafe<Buffer>;
  ~Buffer();

  scoped_ptr<base::SharedMemory> shared_memory_;
  void* memory_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_BUFFER_H_
