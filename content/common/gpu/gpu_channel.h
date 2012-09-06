// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_CHANNEL_H_
#define CONTENT_COMMON_GPU_GPU_CHANNEL_H_

#include <deque>
#include <string>
#include <vector>

#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process.h"
#include "build/build_config.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/message_router.h"
#include "ipc/ipc_sync_channel.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gpu_preference.h"

#if defined(OS_ANDROID)
#include "content/common/android/surface_texture_peer.h"
#endif

class GpuChannelManager;
struct GPUCreateCommandBufferConfig;
class GpuWatchdog;

namespace base {
class MessageLoopProxy;
class WaitableEvent;
}

namespace gpu {
struct RefCountedCounter;
}

#if defined(OS_ANDROID)
namespace content {
class StreamTextureManagerAndroid;
}
#endif

// Encapsulates an IPC channel between the GPU process and one renderer
// process. On the renderer side there's a corresponding GpuChannelHost.
class GpuChannel : public IPC::Listener,
                   public IPC::Sender,
                   public base::RefCountedThreadSafe<GpuChannel> {
 public:
  // Takes ownership of the renderer process handle.
  GpuChannel(GpuChannelManager* gpu_channel_manager,
             GpuWatchdog* watchdog,
             gfx::GLShareGroup* share_group,
             gpu::gles2::MailboxManager* mailbox_manager,
             int client_id,
             bool software);

  bool Init(base::MessageLoopProxy* io_message_loop,
            base::WaitableEvent* shutdown_event);

  // Get the GpuChannelManager that owns this channel.
  GpuChannelManager* gpu_channel_manager() const {
    return gpu_channel_manager_;
  }

  // Returns the name of the associated IPC channel.
  std::string GetChannelName();

#if defined(OS_POSIX)
  int TakeRendererFileDescriptor();
#endif  // defined(OS_POSIX)

  base::ProcessId renderer_pid() const { return channel_->peer_pid(); }

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // IPC::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  virtual void AppendAllCommandBufferStubs(
      std::vector<GpuCommandBufferStubBase*>& stubs);

  // This is called when a command buffer transitions from the unscheduled
  // state to the scheduled state, which potentially means the channel
  // transitions from the unscheduled to the scheduled state. When this occurs
  // deferred IPC messaged are handled.
  void OnScheduled();

  void CreateViewCommandBuffer(
      const gfx::GLSurfaceHandle& window,
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32* route_id);

  gfx::GLShareGroup* share_group() const { return share_group_.get(); }

  GpuCommandBufferStub* LookupCommandBuffer(int32 route_id);

  void LoseAllContexts();

  // Destroy channel and all contained contexts.
  void DestroySoon();

  // Generate a route ID guaranteed to be unique for this channel.
  int GenerateRouteID();

  // Called to add/remove a listener for a particular message routing ID.
  void AddRoute(int32 route_id, IPC::Listener* listener);
  void RemoveRoute(int32 route_id);

  gpu::RefCountedCounter* MessagesPendingCount() {
    return unprocessed_messages_.get();
  }

  // If preempt_by_counter->count is non-zero, any stub on this channel
  // should stop issuing GL commands. Setting this to NULL stops deferral.
  void SetPreemptByCounter(
      scoped_refptr<gpu::RefCountedCounter> preempt_by_counter);

 protected:
  virtual ~GpuChannel();

 private:
  friend class base::RefCountedThreadSafe<GpuChannel>;

  void OnDestroy();

  bool OnControlMessageReceived(const IPC::Message& msg);

  void HandleMessage();

  // Message handlers.
  void OnCreateOffscreenCommandBuffer(
      const gfx::Size& size,
      const GPUCreateCommandBufferConfig& init_params,
      IPC::Message* reply_message);
  void OnDestroyCommandBuffer(int32 route_id, IPC::Message* reply_message);

#if defined(OS_ANDROID)
  // Register the StreamTextureProxy class with the gpu process so that all
  // the callbacks will be correctly forwarded to the renderer.
  void OnRegisterStreamTextureProxy(
      int32 stream_id, const gfx::Size& initial_size, int32* route_id);

  // Create a java surface texture object and send it to the renderer process
  // through binder thread.
  void OnEstablishStreamTexture(
      int32 stream_id, content::SurfaceTexturePeer::SurfaceTextureTarget type,
      int32 primary_id, int32 secondary_id);
#endif

  // Collect rendering stats.
  void OnCollectRenderingStatsForSurface(
      int32 surface_id, IPC::Message* reply_message);

  // The lifetime of objects of this class is managed by a GpuChannelManager.
  // The GpuChannelManager destroy all the GpuChannels that they own when they
  // are destroyed. So a raw pointer is safe.
  GpuChannelManager* gpu_channel_manager_;

  scoped_ptr<IPC::SyncChannel> channel_;

  // Number of routed messages for pending processing on a stub.
  scoped_refptr<gpu::RefCountedCounter> unprocessed_messages_;

  // If non-NULL, all stubs on this channel should stop processing GL
  // commands (via their GpuScheduler) when preempt_by_counter_->count
  // is non-zero.
  scoped_refptr<gpu::RefCountedCounter> preempt_by_counter_;

  std::deque<IPC::Message*> deferred_messages_;

  // The id of the client who is on the other side of the channel.
  int client_id_;

  // Uniquely identifies the channel within this GPU process.
  std::string channel_id_;

  // Used to implement message routing functionality to CommandBuffer objects
  MessageRouter router_;

  // The share group that all contexts associated with a particular renderer
  // process use.
  scoped_refptr<gfx::GLShareGroup> share_group_;

  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;

#if defined(ENABLE_GPU)
  typedef IDMap<GpuCommandBufferStub, IDMapOwnPointer> StubMap;
  StubMap stubs_;
#endif  // defined (ENABLE_GPU)

  bool log_messages_;  // True if we should log sent and received messages.
  gpu::gles2::DisallowedFeatures disallowed_features_;
  GpuWatchdog* watchdog_;
  bool software_;
  bool handle_messages_scheduled_;
  bool processed_get_state_fast_;

#if defined(OS_ANDROID)
  scoped_ptr<content::StreamTextureManagerAndroid> stream_texture_manager_;
#endif

  base::WeakPtrFactory<GpuChannel> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannel);
};

#endif  // CONTENT_COMMON_GPU_GPU_CHANNEL_H_
