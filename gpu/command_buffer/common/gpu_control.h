// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_GPU_CONTROL_H_
#define GPU_COMMAND_BUFFER_COMMON_GPU_CONTROL_H_

#include "gpu/gpu_export.h"

namespace gfx {
class GpuMemoryBuffer;
}

namespace gpu {

// Common interface for GpuControl implementations.
class GPU_EXPORT GpuControl {
 public:
  GpuControl() {}
  virtual ~GpuControl() {}

  // Create a gpu memory buffer of the given dimensions and format. Returns
  // its ID or -1 on error.
  virtual gfx::GpuMemoryBuffer* CreateGpuMemoryBuffer(
      size_t width,
      size_t height,
      unsigned internalformat,
      int32* id) = 0;

  // Destroy a gpu memory buffer. The ID must be positive.
  virtual void DestroyGpuMemoryBuffer(int32 id) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuControl);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_GPU_CONTROL_H_
