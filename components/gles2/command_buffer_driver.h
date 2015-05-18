// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLES2_COMMAND_BUFFER_DRIVER_H_
#define COMPONENTS_GLES2_COMMAND_BUFFER_DRIVER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "components/gpu/public/interfaces/command_buffer.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {
class CommandBufferService;
class GpuScheduler;
class GpuControlService;
class SyncPointManager;
namespace gles2 {
class GLES2Decoder;
class MailboxManager;
}
}

namespace gfx {
class GLContext;
class GLShareGroup;
class GLSurface;
}

namespace gles2 {

// This class receives CommandBuffer messages on the same thread as the native
// viewport. It is usually destructed on that thread, however if the native
// viewport app is destroyed before CommandBufferImpl, then the latter's failed
// PostTask will end up deleting this class on the control thread.
class CommandBufferDriver {
 public:
  class Client {
   public:
    virtual ~Client();
    virtual void UpdateVSyncParameters(base::TimeTicks timebase,
                                       base::TimeDelta interval) = 0;
    virtual void DidLoseContext() = 0;
  };
  // Offscreen.
  CommandBufferDriver(gfx::GLShareGroup* share_group,
                      gpu::gles2::MailboxManager* mailbox_manager,
                      gpu::SyncPointManager* sync_point_manager);
  // Onscreen.
  CommandBufferDriver(
      gfx::AcceleratedWidget widget,
      gfx::GLShareGroup* share_group,
      gpu::gles2::MailboxManager* mailbox_manager,
      gpu::SyncPointManager* sync_point_manager,
      const base::Callback<void(CommandBufferDriver*)>& destruct_callback);
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

  // Called at shutdown to destroy the X window. This is needed when the parent
  // window is being destroyed. Otherwise X calls for this window will fail.
  void DestroyWindow();

 private:
  bool DoInitialize(mojo::ScopedSharedBufferHandle shared_state);
  void OnResize(gfx::Size size, float scale_factor);
  bool OnWaitSyncPoint(uint32_t sync_point);
  void OnSyncPointRetired();
  void OnParseError();
  void OnContextLost(uint32_t reason);
  void OnUpdateVSyncParameters(const base::TimeTicks timebase,
                               const base::TimeDelta interval);
  void DestroyDecoder();

  scoped_ptr<Client> client_;
  mojo::CommandBufferSyncClientPtr sync_client_;
  mojo::CommandBufferLostContextObserverPtr loss_observer_;
  gfx::AcceleratedWidget widget_;
  scoped_ptr<gpu::CommandBufferService> command_buffer_;
  scoped_ptr<gpu::gles2::GLES2Decoder> decoder_;
  scoped_ptr<gpu::GpuScheduler> scheduler_;
  scoped_refptr<gfx::GLContext> context_;
  scoped_refptr<gfx::GLSurface> surface_;
  scoped_refptr<gfx::GLShareGroup> share_group_;
  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;
  scoped_refptr<gpu::SyncPointManager> sync_point_manager_;

  scoped_refptr<base::SingleThreadTaskRunner> context_lost_task_runner_;
  base::Callback<void(int32_t)> context_lost_callback_;

  base::Callback<void(CommandBufferDriver*)> destruct_callback_;

  base::WeakPtrFactory<CommandBufferDriver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferDriver);
};

}  // namespace gles2

#endif  // COMPONENTS_GLES2_COMMAND_BUFFER_DRIVER_H_
