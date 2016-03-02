// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_CHANNEL_MANAGER_H_
#define CONTENT_COMMON_GPU_GPU_CHANNEL_MANAGER_H_

#include <stdint.h>

#include <deque>
#include <string>
#include <vector>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "gpu/command_buffer/common/constants.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface.h"
#include "url/gurl.h"

namespace base {
class WaitableEvent;
}

namespace gfx {
class GLShareGroup;
}

namespace gpu {
struct GpuPreferences;
class PreemptionFlag;
class SyncPointClient;
class SyncPointManager;
struct SyncToken;
union ValueState;
namespace gles2 {
class FramebufferCompletenessCache;
class MailboxManager;
class ProgramCache;
class ShaderTranslatorCache;
}
}

namespace IPC {
struct ChannelHandle;
}

namespace content {
class GpuChannel;
class GpuChannelManagerDelegate;
class GpuMemoryBufferFactory;
class GpuWatchdog;
class ImageTransportHelper;
struct EstablishChannelParams;
#if defined(OS_MACOSX)
struct BufferPresentedParams;
#endif

// A GpuChannelManager is a thread responsible for issuing rendering commands
// managing the lifetimes of GPU channels and forwarding IPC requests from the
// browser process to them based on the corresponding renderer ID.
class CONTENT_EXPORT GpuChannelManager {
 public:
  GpuChannelManager(const gpu::GpuPreferences& gpu_preferences,
                    GpuChannelManagerDelegate* delegate,
                    GpuWatchdog* watchdog,
                    base::SingleThreadTaskRunner* task_runner,
                    base::SingleThreadTaskRunner* io_task_runner,
                    base::WaitableEvent* shutdown_event,
                    gpu::SyncPointManager* sync_point_manager,
                    GpuMemoryBufferFactory* gpu_memory_buffer_factory);
  virtual ~GpuChannelManager();

  GpuChannelManagerDelegate* delegate() const { return delegate_; }

  IPC::ChannelHandle EstablishChannel(const EstablishChannelParams& params);
  void PopulateShaderCache(const std::string& shader);
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id,
                              const gpu::SyncToken& sync_token);
  void UpdateValueState(int client_id,
                        unsigned int target,
                        const gpu::ValueState& state);
#if defined(OS_ANDROID)
  void WakeUpGpu();
#endif
  void DestroyAllChannels();

  // Remove the channel for a particular renderer.
  void RemoveChannel(int client_id);

  void LoseAllContexts();

#if defined(OS_MACOSX)
  void AddImageTransportSurface(int32_t routing_id,
                                ImageTransportHelper* image_transport_helper);
  void RemoveImageTransportSurface(int32_t routing_id);
  void BufferPresented(const BufferPresentedParams& params);
#endif

  const gpu::GpuPreferences& gpu_preferences() const {
    return gpu_preferences_;
  }
  gpu::gles2::ProgramCache* program_cache();
  gpu::gles2::ShaderTranslatorCache* shader_translator_cache();
  gpu::gles2::FramebufferCompletenessCache* framebuffer_completeness_cache();

  GpuMemoryManager* gpu_memory_manager() { return &gpu_memory_manager_; }

  GpuChannel* LookupChannel(int32_t client_id) const;

  gfx::GLSurface* GetDefaultOffscreenSurface();

  GpuMemoryBufferFactory* gpu_memory_buffer_factory() {
    return gpu_memory_buffer_factory_;
  }

  // Returns the maximum order number for unprocessed IPC messages across all
  // channels.
  uint32_t GetUnprocessedOrderNum() const;

  // Returns the maximum order number for processed IPC messages across all
  // channels.
  uint32_t GetProcessedOrderNum() const;

#if defined(OS_ANDROID)
  void DidAccessGpu();
#endif

 protected:
  virtual scoped_ptr<GpuChannel> CreateGpuChannel(
      int client_id,
      uint64_t client_tracing_id,
      bool preempts,
      bool allow_view_command_buffers,
      bool allow_real_time_streams);

  gpu::SyncPointManager* sync_point_manager() const {
    return sync_point_manager_;
  }

  gfx::GLShareGroup* share_group() const { return share_group_.get(); }
  gpu::gles2::MailboxManager* mailbox_manager() const {
    return mailbox_manager_.get();
  }
  gpu::PreemptionFlag* preemption_flag() const {
    return preemption_flag_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // These objects manage channels to individual renderer processes there is
  // one channel for each renderer process that has connected to this GPU
  // process.
  base::ScopedPtrHashMap<int32_t, scoped_ptr<GpuChannel>> gpu_channels_;

 private:
  void InternalDestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id, int client_id);
  void InternalDestroyGpuMemoryBufferOnIO(gfx::GpuMemoryBufferId id,
                                          int client_id);
#if defined(OS_ANDROID)
  void ScheduleWakeUpGpu();
  void DoWakeUpGpu();
#endif

  const gpu::GpuPreferences& gpu_preferences_;

  GpuChannelManagerDelegate* const delegate_;
#if defined(OS_MACOSX)
  IDMap<ImageTransportHelper> image_transport_map_;
#endif

  GpuWatchdog* watchdog_;

  base::WaitableEvent* shutdown_event_;

  scoped_refptr<gfx::GLShareGroup> share_group_;
  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;
  scoped_refptr<gpu::PreemptionFlag> preemption_flag_;
  GpuMemoryManager gpu_memory_manager_;
  // SyncPointManager guaranteed to outlive running MessageLoop.
  gpu::SyncPointManager* sync_point_manager_;
  scoped_ptr<gpu::SyncPointClient> sync_point_client_waiter_;
  scoped_ptr<gpu::gles2::ProgramCache> program_cache_;
  scoped_refptr<gpu::gles2::ShaderTranslatorCache> shader_translator_cache_;
  scoped_refptr<gpu::gles2::FramebufferCompletenessCache>
      framebuffer_completeness_cache_;
  scoped_refptr<gfx::GLSurface> default_offscreen_surface_;
  GpuMemoryBufferFactory* const gpu_memory_buffer_factory_;
#if defined(OS_ANDROID)
  // Last time we know the GPU was powered on. Global for tracking across all
  // transport surfaces.
  base::TimeTicks last_gpu_access_time_;
  base::TimeTicks begin_wake_up_time_;
#endif

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to Controller are invalidated before its members
  // variable's destructors are executed, rendering them invalid.
  base::WeakPtrFactory<GpuChannelManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelManager);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_CHANNEL_MANAGER_H_
