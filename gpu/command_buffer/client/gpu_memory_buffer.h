// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_H_
#define GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "gles2_impl_export.h"

namespace gpu {

// Interface for creating and accessing a zero-copy GPU memory buffer.
// This design evolved from the generalization of GraphicBuffer API
// of Android framework.
//
// THREADING CONSIDERATIONS:
//
// This interface is thread-safe. However, multiple threads mapping
// a buffer for Write or ReadOrWrite simultaneously may result in undefined
// behavior and is not allowed.
class GLES2_IMPL_EXPORT GpuMemoryBuffer {
 public:
  typedef base::Callback<scoped_ptr<GpuMemoryBuffer>(int, int)> Creator;
  enum AccessMode {
    READ_ONLY,
    WRITE_ONLY,
    READ_OR_WRITE,
  };

  // Frees a previously allocated buffer. Freeing a buffer that is still
  // mapped in any process is undefined behavior.
  virtual ~GpuMemoryBuffer() {}

  // Maps the buffer so the client can write the bitmap data in |*vaddr|
  // subsequently. This call may block, for instance if the hardware needs
  // to finish rendering or if CPU caches need to be synchronized.
  virtual void Map(AccessMode mode, void** vaddr) = 0;

  // Unmaps the buffer. Called after all changes to the buffer are
  // completed.
  virtual void Unmap() = 0;

  // Returns true iff the buffer is mapped.
  virtual bool IsMapped() = 0;

  // Returns the native pointer for the buffer.
  virtual void* GetNativeBuffer() = 0;

  // Returns the stride in pixels for the buffer.
  virtual uint32 GetStride() = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_H_
