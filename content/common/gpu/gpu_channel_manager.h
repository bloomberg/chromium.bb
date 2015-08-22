// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_CHANNEL_MANAGER_H_
#define CONTENT_COMMON_GPU_GPU_CHANNEL_MANAGER_H_

#include <deque>
#include <string>
#include <vector>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/message_router.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface.h"

namespace base {
class WaitableEvent;
}

namespace gfx {
class GLShareGroup;
}

namespace gpu {
class SyncPointManager;
union ValueState;
namespace gles2 {
class FramebufferCompletenessCache;
class MailboxManager;
class ProgramCache;
class ShaderTranslatorCache;
}
}

namespace IPC {
class AttachmentBroker;
struct ChannelHandle;
class SyncChannel;
}

struct GPUCreateCommandBufferConfig;

namespace content {
class GpuChannel;
class GpuMemoryBufferFactory;
class GpuWatchdog;

// A GpuChannelManager is a thread responsible for issuing rendering commands
// managing the lifetimes of GPU channels and forwarding IPC requests from the
// browser process to them based on the corresponding renderer ID.
class CONTENT_EXPORT GpuChannelManager : public IPC::Listener,
                          public IPC::Sender {
 public:
  // |broker| must outlive GpuChannelManager and any channels it creates.
  GpuChannelManager(IPC::SyncChannel* channel,
                    GpuWatchdog* watchdog,
                    base::SingleThreadTaskRunner* task_runner,
                    base::SingleThreadTaskRunner* io_task_runner,
                    base::WaitableEvent* shutdown_event,
                    IPC::AttachmentBroker* broker,
                    gpu::SyncPointManager* sync_point_manager,
                    GpuMemoryBufferFactory* gpu_memory_buffer_factory);
  ~GpuChannelManager() override;

  // Remove the channel for a particular renderer.
  void RemoveChannel(int client_id);

  // Listener overrides.
  bool OnMessageReceived(const IPC::Message& msg) override;

  // Sender overrides.
  bool Send(IPC::Message* msg) override;

  bool HandleMessagesScheduled();
  uint64 MessagesProcessed();

  void LoseAllContexts();

  int GenerateRouteID();
  void AddRoute(int32 routing_id, IPC::Listener* listener);
  void RemoveRoute(int32 routing_id);

  gpu::gles2::ProgramCache* program_cache();
  gpu::gles2::ShaderTranslatorCache* shader_translator_cache();
  gpu::gles2::FramebufferCompletenessCache* framebuffer_completeness_cache();

  GpuMemoryManager* gpu_memory_manager() { return &gpu_memory_manager_; }

  GpuChannel* LookupChannel(int32 client_id);

  gpu::SyncPointManager* sync_point_manager() {
    return sync_point_manager_;
  }

  gfx::GLSurface* GetDefaultOffscreenSurface();

  GpuMemoryBufferFactory* gpu_memory_buffer_factory() {
    return gpu_memory_buffer_factory_;
  }

 protected:
  virtual scoped_ptr<GpuChannel> CreateGpuChannel(
      gfx::GLShareGroup* share_group,
      gpu::gles2::MailboxManager* mailbox_manager,
      int client_id,
      uint64_t client_tracing_id,
      bool allow_future_sync_points);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // These objects manage channels to individual renderer processes there is
  // one channel for each renderer process that has connected to this GPU
  // process.
  base::ScopedPtrHashMap<int32, scoped_ptr<GpuChannel>> gpu_channels_;

 private:
  // Message handlers.
  bool OnControlMessageReceived(const IPC::Message& msg);
  void OnEstablishChannel(int client_id,
                          uint64_t client_tracing_id,
                          bool share_context,
                          bool allow_future_sync_points);
  void OnCloseChannel(const IPC::ChannelHandle& channel_handle);
  void OnVisibilityChanged(
      int32 render_view_id, int32 client_id, bool visible);
  void OnCreateViewCommandBuffer(
      const gfx::GLSurfaceHandle& window,
      int32 render_view_id,
      int32 client_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32 route_id);
  void OnLoadedShader(std::string shader);
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id, int client_id);
  void DestroyGpuMemoryBufferOnIO(gfx::GpuMemoryBufferId id, int client_id);
  void OnDestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                int client_id,
                                int32 sync_point);

  void OnUpdateValueState(int client_id,
                          unsigned int target,
                          const gpu::ValueState& state);
  void OnLoseAllContexts();

  // Used to send and receive IPC messages from the browser process.
  IPC::SyncChannel* const channel_;
  MessageRouter router_;

  GpuWatchdog* watchdog_;

  base::WaitableEvent* shutdown_event_;

  scoped_refptr<gfx::GLShareGroup> share_group_;
  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;
  GpuMemoryManager gpu_memory_manager_;
  // SyncPointManager guaranteed to outlive running MessageLoop.
  gpu::SyncPointManager* sync_point_manager_;
  scoped_ptr<gpu::gles2::ProgramCache> program_cache_;
  scoped_refptr<gpu::gles2::ShaderTranslatorCache> shader_translator_cache_;
  scoped_refptr<gpu::gles2::FramebufferCompletenessCache>
      framebuffer_completeness_cache_;
  scoped_refptr<gfx::GLSurface> default_offscreen_surface_;
  GpuMemoryBufferFactory* const gpu_memory_buffer_factory_;
  // Must outlive this instance of GpuChannelManager.
  IPC::AttachmentBroker* attachment_broker_;

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to Controller are invalidated before its members
  // variable's destructors are executed, rendering them invalid.
  base::WeakPtrFactory<GpuChannelManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelManager);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_CHANNEL_MANAGER_H_
