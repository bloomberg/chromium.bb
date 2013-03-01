// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_CHANNEL_H_
#define CONTENT_COMMON_GPU_GPU_CHANNEL_H_

#include <deque>
#include <string>

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

struct GPUCreateCommandBufferConfig;

namespace base {
class MessageLoopProxy;
class WaitableEvent;
}

namespace gpu {
class PreemptionFlag;
namespace gles2 {
class ImageManager;
}
}

#if defined(OS_ANDROID)
namespace content {
class StreamTextureManagerAndroid;
}
#endif

namespace content {
class GpuChannelManager;
struct GpuRenderingStats;
class GpuWatchdog;
class SyncPointMessageFilter;

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

  // Requeue the message that is currently being processed to the beginning of
  // the queue. Used when the processing of a message gets aborted because of
  // unscheduling conditions.
  void RequeueMessage();

  // This is called when a command buffer transitions from the unscheduled
  // state to the scheduled state, which potentially means the channel
  // transitions from the unscheduled to the scheduled state. When this occurs
  // deferred IPC messaged are handled.
  void OnScheduled();

  // This is called when a command buffer transitions between scheduled and
  // descheduled states. When any stub is descheduled, we stop preempting
  // other channels.
  void StubSchedulingChanged(bool scheduled);

  void CreateViewCommandBuffer(
      const gfx::GLSurfaceHandle& window,
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32* route_id);

  void CreateImage(
      gfx::PluginWindowHandle window,
      int32 image_id,
      gfx::Size* size);
  void DeleteImage(int32 image_id);

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

  gpu::PreemptionFlag* GetPreemptionFlag();

  // If |preemption_flag->IsSet()|, any stub on this channel
  // should stop issuing GL commands. Setting this to NULL stops deferral.
  void SetPreemptByFlag(
      scoped_refptr<gpu::PreemptionFlag> preemption_flag);

#if defined(OS_ANDROID)
  StreamTextureManagerAndroid* stream_texture_manager() {
    return stream_texture_manager_.get();
  }
#endif

 protected:
  virtual ~GpuChannel();

 private:
  friend class base::RefCountedThreadSafe<GpuChannel>;
  friend class SyncPointMessageFilter;

  void OnDestroy();

  bool OnControlMessageReceived(const IPC::Message& msg);

  void HandleMessage();

  // Message handlers.
  void OnCreateOffscreenCommandBuffer(
      const gfx::Size& size,
      const GPUCreateCommandBufferConfig& init_params,
      int32* route_id);
  void OnDestroyCommandBuffer(int32 route_id);

#if defined(OS_ANDROID)
  // Register the StreamTextureProxy class with the gpu process so that all
  // the callbacks will be correctly forwarded to the renderer.
  void OnRegisterStreamTextureProxy(
      int32 stream_id, const gfx::Size& initial_size, int32* route_id);

  // Create a java surface texture object and send it to the renderer process
  // through binder thread.
  void OnEstablishStreamTexture(
      int32 stream_id, SurfaceTexturePeer::SurfaceTextureTarget type,
      int32 primary_id, int32 secondary_id);
#endif

  // Collect rendering stats.
  void OnCollectRenderingStatsForSurface(
      int32 surface_id, GpuRenderingStats* stats);

  // Decrement the count of unhandled IPC messages and defer preemption.
  void MessageProcessed();

  // The lifetime of objects of this class is managed by a GpuChannelManager.
  // The GpuChannelManager destroy all the GpuChannels that they own when they
  // are destroyed. So a raw pointer is safe.
  GpuChannelManager* gpu_channel_manager_;

  scoped_ptr<IPC::SyncChannel> channel_;

  uint64 messages_processed_;

  // Whether the processing of IPCs on this channel is stalled and we should
  // preempt other GpuChannels.
  scoped_refptr<gpu::PreemptionFlag> preempting_flag_;

  // If non-NULL, all stubs on this channel should stop processing GL
  // commands (via their GpuScheduler) when preempted_flag_->IsSet()
  scoped_refptr<gpu::PreemptionFlag> preempted_flag_;

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
  scoped_refptr<gpu::gles2::ImageManager> image_manager_;

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
  IPC::Message* currently_processing_message_;

#if defined(OS_ANDROID)
  scoped_ptr<StreamTextureManagerAndroid> stream_texture_manager_;
#endif

  base::WeakPtrFactory<GpuChannel> weak_factory_;

  scoped_refptr<SyncPointMessageFilter> filter_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  size_t num_stubs_descheduled_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannel);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_CHANNEL_H_
