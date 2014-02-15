// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_CONTROL_SERVICE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_CONTROL_SERVICE_H_

#include <map>

#include "base/memory/linked_ptr.h"
#include "gpu/command_buffer/common/gpu_control.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {
class GpuMemoryBufferFactory;
class GpuMemoryBufferManagerInterface;

namespace gles2 {
class MailboxManager;
class QueryManager;
}

class GPU_EXPORT GpuControlService : public GpuControl {
 public:
  GpuControlService(GpuMemoryBufferManagerInterface* gpu_memory_buffer_manager,
                    GpuMemoryBufferFactory* gpu_memory_buffer_factory,
                    gles2::MailboxManager* mailbox_manager,
                    gles2::QueryManager* query_manager,
                    const gpu::Capabilities& decoder_capabilities);
  virtual ~GpuControlService();


  // GpuControl implementation.
  virtual gpu::Capabilities GetCapabilities() OVERRIDE;
  virtual gfx::GpuMemoryBuffer* CreateGpuMemoryBuffer(
      size_t width,
      size_t height,
      unsigned internalformat,
      int32* id) OVERRIDE;
  virtual void DestroyGpuMemoryBuffer(int32 id) OVERRIDE;
  virtual uint32 InsertSyncPoint() OVERRIDE;
  virtual void SignalSyncPoint(uint32 sync_point,
                               const base::Closure& callback) OVERRIDE;
  virtual void SignalQuery(uint32 query,
                           const base::Closure& callback) OVERRIDE;
  virtual void SetSurfaceVisible(bool visible) OVERRIDE;
  virtual void SendManagedMemoryStats(const ManagedMemoryStats& stats)
      OVERRIDE;
  virtual void Echo(const base::Closure& callback) OVERRIDE;
  virtual uint32 CreateStreamTexture(uint32 texture_id) OVERRIDE;

  // Register an existing gpu memory buffer and get an ID that can be used
  // to identify it in the command buffer.
  bool RegisterGpuMemoryBuffer(int32 id,
                               gfx::GpuMemoryBufferHandle buffer,
                               size_t width,
                               size_t height,
                               unsigned internalformat);

 private:
  GpuMemoryBufferManagerInterface* gpu_memory_buffer_manager_;
  GpuMemoryBufferFactory* gpu_memory_buffer_factory_;
  gles2::MailboxManager* mailbox_manager_;
  gles2::QueryManager* query_manager_;
  typedef std::map<int32, linked_ptr<gfx::GpuMemoryBuffer> > GpuMemoryBufferMap;
  GpuMemoryBufferMap gpu_memory_buffers_;
  gpu::Capabilities capabilities_;

  DISALLOW_COPY_AND_ASSIGN(GpuControlService);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_CONTROL_SERVICE_H_
