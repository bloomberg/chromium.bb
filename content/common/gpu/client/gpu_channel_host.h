// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_CHANNEL_HOST_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_CHANNEL_HOST_H_

#include <string>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/gpu/gpu_result_codes.h"
#include "content/common/message_router.h"
#include "gpu/config/gpu_info.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/message_filter.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gpu_preference.h"

class GURL;
class TransportTextureService;
struct GPUCreateCommandBufferConfig;

namespace base {
class MessageLoop;
class MessageLoopProxy;
class WaitableEvent;
}

namespace IPC {
class SyncMessageFilter;
}

namespace media {
class VideoDecodeAccelerator;
class VideoEncodeAccelerator;
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace content {
class CommandBufferProxyImpl;
class GpuChannelHost;

struct GpuListenerInfo {
  GpuListenerInfo();
  ~GpuListenerInfo();

  base::WeakPtr<IPC::Listener> listener;
  scoped_refptr<base::MessageLoopProxy> loop;
};

struct ProxyFlushInfo {
  ProxyFlushInfo();
  ~ProxyFlushInfo();

  bool flush_pending;
  int route_id;
  int32 put_offset;
  unsigned int flush_count;
  std::vector<ui::LatencyInfo> latency_info;
};

class CONTENT_EXPORT GpuChannelHostFactory {
 public:
  virtual ~GpuChannelHostFactory() {}

  virtual bool IsMainThread() = 0;
  virtual base::MessageLoop* GetMainLoop() = 0;
  virtual scoped_refptr<base::MessageLoopProxy> GetIOLoopProxy() = 0;
  virtual scoped_ptr<base::SharedMemory> AllocateSharedMemory(size_t size) = 0;
  virtual CreateCommandBufferResult CreateViewCommandBuffer(
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32 route_id) = 0;
};

// Encapsulates an IPC channel between the client and one GPU process.
// On the GPU process side there's a corresponding GpuChannel.
// Every method can be called on any thread with a message loop, except for the
// IO thread.
class GpuChannelHost : public IPC::Sender,
                       public base::RefCountedThreadSafe<GpuChannelHost> {
 public:
  // Must be called on the main thread (as defined by the factory).
  static scoped_refptr<GpuChannelHost> Create(
      GpuChannelHostFactory* factory,
      const gpu::GPUInfo& gpu_info,
      const IPC::ChannelHandle& channel_handle,
      base::WaitableEvent* shutdown_event,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);

  bool IsLost() const {
    DCHECK(channel_filter_.get());
    return channel_filter_->IsLost();
  }

  // The GPU stats reported by the GPU process.
  const gpu::GPUInfo& gpu_info() const { return gpu_info_; }

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  // Set an ordering barrier.  AsyncFlushes any pending barriers on other
  // routes. Combines multiple OrderingBarriers into a single AsyncFlush.
  void OrderingBarrier(int route_id,
                       int32 put_offset,
                       unsigned int flush_count,
                       const std::vector<ui::LatencyInfo>& latency_info,
                       bool put_offset_changed,
                       bool do_flush);

  // Create and connect to a command buffer in the GPU process.
  CommandBufferProxyImpl* CreateViewCommandBuffer(
      int32 surface_id,
      CommandBufferProxyImpl* share_group,
      const std::vector<int32>& attribs,
      const GURL& active_url,
      gfx::GpuPreference gpu_preference);

  // Create and connect to a command buffer in the GPU process.
  CommandBufferProxyImpl* CreateOffscreenCommandBuffer(
      const gfx::Size& size,
      CommandBufferProxyImpl* share_group,
      const std::vector<int32>& attribs,
      const GURL& active_url,
      gfx::GpuPreference gpu_preference);

  // Creates a video decoder in the GPU process.
  scoped_ptr<media::VideoDecodeAccelerator> CreateVideoDecoder(
      int command_buffer_route_id);

  // Creates a video encoder in the GPU process.
  scoped_ptr<media::VideoEncodeAccelerator> CreateVideoEncoder(
      int command_buffer_route_id);

  // Destroy a command buffer created by this channel.
  void DestroyCommandBuffer(CommandBufferProxyImpl* command_buffer);

  // Destroy this channel.
  void DestroyChannel();

  // Add a route for the current message loop.
  void AddRoute(int route_id, base::WeakPtr<IPC::Listener> listener);
  void RemoveRoute(int route_id);

  GpuChannelHostFactory* factory() const { return factory_; }

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() const {
    return gpu_memory_buffer_manager_;
  }

  // Returns a handle to the shared memory that can be sent via IPC to the
  // GPU process. The caller is responsible for ensuring it is closed. Returns
  // an invalid handle on failure.
  base::SharedMemoryHandle ShareToGpuProcess(
      base::SharedMemoryHandle source_handle);

  // Reserve one unused transfer buffer ID.
  int32 ReserveTransferBufferId();

  // Returns a GPU memory buffer handle to the buffer that can be sent via
  // IPC to the GPU process. The caller is responsible for ensuring it is
  // closed. Returns an invalid handle on failure.
  gfx::GpuMemoryBufferHandle ShareGpuMemoryBufferToGpuProcess(
      const gfx::GpuMemoryBufferHandle& source_handle,
      bool* requires_sync_point);

  // Reserve one unused image ID.
  int32 ReserveImageId();

  // Generate a route ID guaranteed to be unique for this channel.
  int32 GenerateRouteID();

 private:
  friend class base::RefCountedThreadSafe<GpuChannelHost>;
  GpuChannelHost(GpuChannelHostFactory* factory,
                 const gpu::GPUInfo& gpu_info,
                 gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);
  ~GpuChannelHost() override;
  void Connect(const IPC::ChannelHandle& channel_handle,
               base::WaitableEvent* shutdown_event);
  bool InternalSend(IPC::Message* msg);
  void InternalFlush();

  // A filter used internally to route incoming messages from the IO thread
  // to the correct message loop. It also maintains some shared state between
  // all the contexts.
  class MessageFilter : public IPC::MessageFilter {
   public:
    MessageFilter();

    // Called on the IO thread.
    void AddRoute(int route_id,
                  base::WeakPtr<IPC::Listener> listener,
                  scoped_refptr<base::MessageLoopProxy> loop);
    // Called on the IO thread.
    void RemoveRoute(int route_id);

    // IPC::MessageFilter implementation
    // (called on the IO thread):
    bool OnMessageReceived(const IPC::Message& msg) override;
    void OnChannelError() override;

    // The following methods can be called on any thread.

    // Whether the channel is lost.
    bool IsLost() const;

   private:
    ~MessageFilter() override;

    // Threading notes: |listeners_| is only accessed on the IO thread. Every
    // other field is protected by |lock_|.
    typedef base::hash_map<int, GpuListenerInfo> ListenerMap;
    ListenerMap listeners_;

    // Protects all fields below this one.
    mutable base::Lock lock_;

    // Whether the channel has been lost.
    bool lost_;
  };

  // Threading notes: all fields are constant during the lifetime of |this|
  // except:
  // - |next_transfer_buffer_id_|, atomic type
  // - |next_image_id_|, atomic type
  // - |next_route_id_|, atomic type
  // - |proxies_|, protected by |context_lock_|
  GpuChannelHostFactory* const factory_;

  const gpu::GPUInfo gpu_info_;

  scoped_ptr<IPC::SyncChannel> channel_;
  scoped_refptr<MessageFilter> channel_filter_;

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;

  // A filter for sending messages from thread other than the main thread.
  scoped_refptr<IPC::SyncMessageFilter> sync_filter_;

  // Transfer buffer IDs are allocated in sequence.
  base::AtomicSequenceNumber next_transfer_buffer_id_;

  // Image IDs are allocated in sequence.
  base::AtomicSequenceNumber next_image_id_;

  // Route IDs are allocated in sequence.
  base::AtomicSequenceNumber next_route_id_;

  // Protects proxies_.
  mutable base::Lock context_lock_;
  // Used to look up a proxy from its routing id.
  typedef base::hash_map<int, CommandBufferProxyImpl*> ProxyMap;
  ProxyMap proxies_;
  ProxyFlushInfo flush_info_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelHost);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_CHANNEL_HOST_H_
