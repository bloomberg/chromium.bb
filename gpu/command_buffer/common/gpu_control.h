// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_GPU_CONTROL_H_
#define GPU_COMMAND_BUFFER_COMMON_GPU_CONTROL_H_

#include <vector>

#include "base/callback.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/types.h"
#include "gpu/gpu_export.h"

namespace gfx {
class GpuMemoryBuffer;
}

namespace gpu {
struct ManagedMemoryStats;

// Common interface for GpuControl implementations.
class GPU_EXPORT GpuControl {
 public:
  GpuControl() {}
  virtual ~GpuControl() {}

  virtual Capabilities GetCapabilities() = 0;

  // Create a gpu memory buffer of the given dimensions and format. Returns
  // its ID or -1 on error.
  virtual gfx::GpuMemoryBuffer* CreateGpuMemoryBuffer(
      size_t width,
      size_t height,
      unsigned internalformat,
      int32* id) = 0;

  // Destroy a gpu memory buffer. The ID must be positive.
  virtual void DestroyGpuMemoryBuffer(int32 id) = 0;

  // Generates n unique mailbox names that can be used with
  // GL_texture_mailbox_CHROMIUM.
  virtual bool GenerateMailboxNames(unsigned num,
                                    std::vector<gpu::Mailbox>* names) = 0;

  // Inserts a sync point, returning its ID. Sync point IDs are global and can
  // be used for cross-context synchronization.
  virtual uint32 InsertSyncPoint() = 0;

  // Runs |callback| when a sync point is reached.
  virtual void SignalSyncPoint(uint32 sync_point,
                               const base::Closure& callback) = 0;

  // Runs |callback| when a query created via glCreateQueryEXT() has cleared
  // passed the glEndQueryEXT() point.
  virtual void SignalQuery(uint32 query, const base::Closure& callback) = 0;

  virtual void SetSurfaceVisible(bool visible) = 0;

  virtual void SendManagedMemoryStats(const ManagedMemoryStats& stats) = 0;

  // Invokes the callback once the context has been flushed.
  virtual void Echo(const base::Closure& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuControl);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_GPU_CONTROL_H_
