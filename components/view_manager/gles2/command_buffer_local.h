// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_GLES2_COMMAND_BUFFER_LOCAL_H_
#define COMPONENTS_VIEW_MANAGER_GLES2_COMMAND_BUFFER_LOCAL_H_

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/view_manager/gles2/gpu_state.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {
class CommandBuffer;
class CommandBufferService;
class GpuScheduler;
class GpuControlService;
namespace gles2 {
class GLES2Decoder;
}
}

namespace gfx {
class GLContext;
class GLSurface;
}

namespace gles2 {

class CommandBufferLocalClient;

// This class provides a thin wrapper around a CommandBufferService and a
// GpuControl implementation to allow cc::Display to generate GL directly on
// the same thread.
class CommandBufferLocal : public gpu::GpuControl {
 public:
  CommandBufferLocal(CommandBufferLocalClient* client,
                     gfx::AcceleratedWidget widget,
                     const scoped_refptr<gles2::GpuState>& gpu_state);
  ~CommandBufferLocal() override;

  bool Initialize();

  gpu::CommandBuffer* GetCommandBuffer();

  // gpu::GpuControl implementation:
  gpu::Capabilities GetCapabilities() override;
  int32_t CreateImage(ClientBuffer buffer,
                      size_t width,
                      size_t height,
                      unsigned internalformat) override;
  void DestroyImage(int32_t id) override;
  int32_t CreateGpuMemoryBufferImage(size_t width,
                                     size_t height,
                                     unsigned internalformat,
                                     unsigned usage) override;
  uint32 InsertSyncPoint() override;
  uint32 InsertFutureSyncPoint() override;
  void RetireSyncPoint(uint32 sync_point) override;
  void SignalSyncPoint(uint32 sync_point,
                       const base::Closure& callback) override;
  void SignalQuery(uint32 query, const base::Closure& callback) override;
  void SetSurfaceVisible(bool visible) override;
  uint32 CreateStreamTexture(uint32 texture_id) override;
  void SetLock(base::Lock*) override;
  bool IsGpuChannelLost() override;

 private:
  void PumpCommands();

  void OnResize(gfx::Size size, float scale_factor);
  void OnUpdateVSyncParameters(const base::TimeTicks timebase,
                               const base::TimeDelta interval);
  bool OnWaitSyncPoint(uint32_t sync_point);
  void OnParseError();
  void OnContextLost(uint32_t reason);
  void OnSyncPointRetired();

  gfx::AcceleratedWidget widget_;
  scoped_refptr<gles2::GpuState> gpu_state_;
  scoped_ptr<gpu::CommandBufferService> command_buffer_;
  scoped_ptr<gpu::GpuScheduler> scheduler_;
  scoped_ptr<gpu::gles2::GLES2Decoder> decoder_;
  scoped_refptr<gfx::GLContext> context_;
  scoped_refptr<gfx::GLSurface> surface_;
  CommandBufferLocalClient* client_;

  base::WeakPtrFactory<CommandBufferLocal> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferLocal);
};

}  //  namespace gles2

#endif  // COMPONENTS_VIEW_MANAGER_GLES2_COMMAND_BUFFER_LOCAL_H_
