// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_CHANNEL_HOST_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_CHANNEL_HOST_H_

#include <string>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/gpu_video_decode_accelerator_host.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/message_router.h"
#include "content/public/common/gpu_info.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_sync_channel.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/gl/gpu_preference.h"

class GURL;
class MessageLoop;
class TransportTextureService;
struct GPUCreateCommandBufferConfig;

namespace base {
class MessageLoopProxy;
}

namespace IPC {
class SyncMessageFilter;
}

namespace content {
class CommandBufferProxyImpl;
struct GpuRenderingStats;

struct GpuListenerInfo {
  GpuListenerInfo();
  ~GpuListenerInfo();

  base::WeakPtr<IPC::Listener> listener;
  scoped_refptr<base::MessageLoopProxy> loop;
};

class CONTENT_EXPORT GpuChannelHostFactory {
 public:
  typedef base::Callback<void(const gfx::Size)> CreateImageCallback;

  virtual ~GpuChannelHostFactory() {}

  virtual bool IsMainThread() = 0;
  virtual bool IsIOThread() = 0;
  virtual MessageLoop* GetMainLoop() = 0;
  virtual scoped_refptr<base::MessageLoopProxy> GetIOLoopProxy() = 0;
  virtual base::WaitableEvent* GetShutDownEvent() = 0;
  virtual scoped_ptr<base::SharedMemory> AllocateSharedMemory(size_t size) = 0;
  virtual int32 CreateViewCommandBuffer(
      int32 surface_id, const GPUCreateCommandBufferConfig& init_params) = 0;
  virtual GpuChannelHost* EstablishGpuChannelSync(CauseForGpuLaunch) = 0;
  virtual void CreateImage(
      gfx::PluginWindowHandle window,
      int32 image_id,
      const CreateImageCallback& callback) = 0;
  virtual void DeleteImage(int32 image_id, int32 sync_point) = 0;
};

// Encapsulates an IPC channel between the client and one GPU process.
// On the GPU process side there's a corresponding GpuChannel.
class GpuChannelHost : public IPC::Sender,
                       public base::RefCountedThreadSafe<GpuChannelHost> {
 public:
  enum State {
    // Not yet connected.
    kUnconnected,
    // Ready to use.
    kConnected,
    // An error caused the host to become disconnected. Recreate channel to
    // reestablish connection.
    kLost
  };

  // Called on the render thread
  GpuChannelHost(GpuChannelHostFactory* factory,
                 int gpu_host_id,
                 int client_id);

  // Connect to GPU process channel.
  void Connect(const IPC::ChannelHandle& channel_handle);

  State state() const { return state_; }

  // Change state to kLost.
  void SetStateLost();

  // The GPU stats reported by the GPU process.
  void set_gpu_info(const GPUInfo& gpu_info);
  const GPUInfo& gpu_info() const;

  void OnMessageReceived(const IPC::Message& message);
  void OnChannelError();

  // IPC::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // Create and connect to a command buffer in the GPU process.
  CommandBufferProxyImpl* CreateViewCommandBuffer(
      int32 surface_id,
      CommandBufferProxyImpl* share_group,
      const std::string& allowed_extensions,
      const std::vector<int32>& attribs,
      const GURL& active_url,
      gfx::GpuPreference gpu_preference);

  // Create and connect to a command buffer in the GPU process.
  CommandBufferProxyImpl* CreateOffscreenCommandBuffer(
      const gfx::Size& size,
      CommandBufferProxyImpl* share_group,
      const std::string& allowed_extensions,
      const std::vector<int32>& attribs,
      const GURL& active_url,
      gfx::GpuPreference gpu_preference);

  // Creates a video decoder in the GPU process.
  // Returned pointer is owned by the CommandBufferProxy for |route_id|.
  GpuVideoDecodeAcceleratorHost* CreateVideoDecoder(
      int command_buffer_route_id,
      media::VideoCodecProfile profile,
      media::VideoDecodeAccelerator::Client* client);

  // Destroy a command buffer created by this channel.
  void DestroyCommandBuffer(CommandBufferProxyImpl* command_buffer);

  // Collect rendering stats from GPU process.
  bool CollectRenderingStatsForSurface(
      int surface_id, GpuRenderingStats* stats);

  // Add a route for the current message loop.
  void AddRoute(int route_id, base::WeakPtr<IPC::Listener> listener);
  void RemoveRoute(int route_id);

  GpuChannelHostFactory* factory() const { return factory_; }
  int gpu_host_id() const { return gpu_host_id_; }

  // Do not use this function! It does not take the context lock and even
  // if it did the PID might become invalid immediately after releasing it
  // TODO(apatrick): Make all callers use ShareToGpuProcess().
  base::ProcessId gpu_pid() const { return channel_->peer_pid(); }

  int client_id() const { return client_id_; }

  // Returns a handle to the shared memory that can be sent via IPC to the
  // GPU process. The caller is responsible for ensuring it is closed. Returns
  // an invalid handle on failure.
  base::SharedMemoryHandle ShareToGpuProcess(
      base::SharedMemory* shared_memory);

  // Generates n unique mailbox names that can be used with
  // GL_texture_mailbox_CHROMIUM. Unlike genMailboxCHROMIUM, this IPC is
  // handled only on the GPU process' IO thread, and so is not effectively
  // a finish.
  bool GenerateMailboxNames(unsigned num, std::vector<std::string>* names);

  // Reserve one unused transfer buffer ID.
  int32 ReserveTransferBufferId();

 private:
  friend class base::RefCountedThreadSafe<GpuChannelHost>;
  virtual ~GpuChannelHost();

  // Message handlers.
  void OnGenerateMailboxNamesReply(const std::vector<std::string>& names);

  // A filter used internally to route incoming messages from the IO thread
  // to the correct message loop.
  class MessageFilter : public IPC::ChannelProxy::MessageFilter {
   public:
    explicit MessageFilter(GpuChannelHost* parent);

    void AddRoute(int route_id,
                  base::WeakPtr<IPC::Listener> listener,
                  scoped_refptr<base::MessageLoopProxy> loop);
    void RemoveRoute(int route_id);

    // IPC::ChannelProxy::MessageFilter implementation:
    virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
    virtual void OnChannelError() OVERRIDE;

   private:
    virtual ~MessageFilter();

    GpuChannelHost* parent_;

    typedef base::hash_map<int, GpuListenerInfo> ListenerMap;
    ListenerMap listeners_;
  };

  GpuChannelHostFactory* factory_;
  int client_id_;
  int gpu_host_id_;

  State state_;

  GPUInfo gpu_info_;

  scoped_ptr<IPC::SyncChannel> channel_;
  scoped_refptr<MessageFilter> channel_filter_;

  // Used to look up a proxy from its routing id.
  typedef base::hash_map<int, CommandBufferProxyImpl*> ProxyMap;
  ProxyMap proxies_;

  // A lock to guard against concurrent access to members like the proxies map
  // for calls from contexts that may live on the compositor or main thread.
  mutable base::Lock context_lock_;

  // A filter for sending messages from thread other than the main thread.
  scoped_refptr<IPC::SyncMessageFilter> sync_filter_;

  // A pool of valid mailbox names.
  std::vector<std::string> mailbox_name_pool_;

  // Transfer buffer IDs are allocated in sequence.
  base::AtomicSequenceNumber next_transfer_buffer_id_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelHost);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_CHANNEL_HOST_H_
