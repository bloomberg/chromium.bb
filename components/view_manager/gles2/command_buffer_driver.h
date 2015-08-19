// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_GLES2_COMMAND_BUFFER_DRIVER_H_
#define COMPONENTS_VIEW_MANAGER_GLES2_COMMAND_BUFFER_DRIVER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "components/view_manager/public/interfaces/command_buffer.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
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

class GpuState;

// This class receives CommandBuffer messages on the same thread as the native
// viewport.
class CommandBufferDriver {
 public:
  class Client {
   public:
    virtual ~Client();
    virtual void DidLoseContext() = 0;
  };
  explicit CommandBufferDriver(scoped_refptr<GpuState> gpu_state);

  ~CommandBufferDriver();

  void set_client(scoped_ptr<Client> client) { client_ = client.Pass(); }

  void Initialize(mojo::CommandBufferSyncClientPtr sync_client,
                  mojo::CommandBufferLostContextObserverPtr loss_observer,
                  mojo::ScopedSharedBufferHandle shared_state);
  void SetGetBuffer(int32_t buffer);
  void Flush(int32_t put_offset);
  void MakeProgress(int32_t last_get_offset);
  void RegisterTransferBuffer(int32_t id,
                              mojo::ScopedSharedBufferHandle transfer_buffer,
                              uint32_t size);
  void DestroyTransferBuffer(int32_t id);
  void Echo(const mojo::Callback<void()>& callback);
  void CreateImage(int32_t id,
                   mojo::ScopedHandle memory_handle,
                   int32_t type,
                   mojo::SizePtr size,
                   int32_t format,
                   int32_t internal_format);
  void DestroyImage(int32_t id);

 private:
  bool MakeCurrent();
  bool DoInitialize(mojo::ScopedSharedBufferHandle shared_state);
  void OnResize(gfx::Size size, float scale_factor);
  bool OnWaitSyncPoint(uint32_t sync_point);
  void OnSyncPointRetired();
  void OnParseError();
  void OnContextLost(uint32_t reason);
  void DestroyDecoder();

  scoped_ptr<Client> client_;
  mojo::CommandBufferSyncClientPtr sync_client_;
  mojo::CommandBufferLostContextObserverPtr loss_observer_;
  scoped_ptr<gpu::CommandBufferService> command_buffer_;
  scoped_ptr<gpu::gles2::GLES2Decoder> decoder_;
  scoped_ptr<gpu::GpuScheduler> scheduler_;
  scoped_refptr<gfx::GLContext> context_;
  scoped_refptr<gfx::GLSurface> surface_;
  scoped_refptr<GpuState> gpu_state_;

  scoped_refptr<base::SingleThreadTaskRunner> context_lost_task_runner_;
  base::Callback<void(int32_t)> context_lost_callback_;

  base::WeakPtrFactory<CommandBufferDriver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferDriver);
};

}  // namespace gles2

#endif  // COMPONENTS_GLES2_COMMAND_BUFFER_DRIVER_H_
