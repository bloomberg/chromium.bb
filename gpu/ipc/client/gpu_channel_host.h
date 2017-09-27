// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_GPU_CHANNEL_HOST_H_
#define GPU_IPC_CLIENT_GPU_CHANNEL_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/flush_params.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/message_filter.h"
#include "ipc/message_router.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/latency/latency_info.h"

namespace base {
class WaitableEvent;
}

namespace IPC {
struct PendingSyncMsg;
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace gpu {
struct SyncToken;
class GpuChannelHost;
using GpuChannelEstablishedCallback =
    base::Callback<void(scoped_refptr<GpuChannelHost>)>;

class GPU_EXPORT GpuChannelEstablishFactory {
 public:
  virtual ~GpuChannelEstablishFactory() {}

  virtual void EstablishGpuChannel(
      const GpuChannelEstablishedCallback& callback) = 0;
  virtual scoped_refptr<GpuChannelHost> EstablishGpuChannelSync() = 0;
  virtual GpuMemoryBufferManager* GetGpuMemoryBufferManager() = 0;
};

class GPU_EXPORT GpuChannelHostFactory {
 public:
  virtual ~GpuChannelHostFactory() {}

  virtual bool IsMainThread() = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetIOThreadTaskRunner() = 0;
  virtual std::unique_ptr<base::SharedMemory> AllocateSharedMemory(
      size_t size) = 0;
};

// Encapsulates an IPC channel between the client and one GPU process.
// On the GPU process side there's a corresponding GpuChannel.
// Every method can be called on any thread with a message loop, except for the
// IO thread.
class GPU_EXPORT GpuChannelHost
    : public IPC::Sender,
      public base::RefCountedThreadSafe<GpuChannelHost> {
 public:
  // Must be called on the main thread (as defined by the factory).
  static scoped_refptr<GpuChannelHost> Create(
      GpuChannelHostFactory* factory,
      int channel_id,
      const gpu::GPUInfo& gpu_info,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const IPC::ChannelHandle& channel_handle,
      base::WaitableEvent* shutdown_event,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);

  bool IsLost() const {
    DCHECK(channel_filter_.get());
    return channel_filter_->IsLost();
  }

  int channel_id() const { return channel_id_; }

  // The GPU stats reported by the GPU process.
  const gpu::GPUInfo& gpu_info() const { return gpu_info_; }
  const gpu::GpuFeatureInfo& gpu_feature_info() const {
    return gpu_feature_info_;
  }

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  // Enqueue an ordering barrier to defer the flush and return an identifier
  // that can be used to ensure or verify the flush later.
  uint32_t OrderingBarrier(int32_t route_id,
                           int32_t put_offset,
                           std::vector<ui::LatencyInfo> latency_info,
                           std::vector<SyncToken> sync_token_fences);

  // Ensure that the all ordering barriers prior upto |flush_id| have been
  // flushed. Pass UINT32_MAX to force all pending ordering barriers to be
  // flushed.
  void EnsureFlush(uint32_t flush_id);

  // Verify that the all ordering barriers prior upto |flush_id| have reached
  // the service. Pass UINT32_MAX to force all pending ordering barriers to be
  // verified.
  void VerifyFlush(uint32_t flush_id);

  // Destroy this channel. Must be called on the main thread, before
  // destruction.
  void DestroyChannel();

  // Add a message route for the current message loop.
  void AddRoute(int route_id, base::WeakPtr<IPC::Listener> listener);

  // Add a message route to be handled on the provided |task_runner|.
  void AddRouteWithTaskRunner(
      int route_id,
      base::WeakPtr<IPC::Listener> listener,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Remove the message route associated with |route_id|.
  void RemoveRoute(int route_id);

  GpuChannelHostFactory* factory() const { return factory_; }

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() const {
    return gpu_memory_buffer_manager_;
  }

  // Returns a handle to the shared memory that can be sent via IPC to the
  // GPU process. The caller is responsible for ensuring it is closed. Returns
  // an invalid handle on failure.
  base::SharedMemoryHandle ShareToGpuProcess(
      const base::SharedMemoryHandle& source_handle);

  // Reserve one unused transfer buffer ID.
  int32_t ReserveTransferBufferId();

  // Reserve one unused image ID.
  int32_t ReserveImageId();

  // Generate a route ID guaranteed to be unique for this channel.
  int32_t GenerateRouteID();

 private:
  friend class base::RefCountedThreadSafe<GpuChannelHost>;

  // A filter used internally to route incoming messages from the IO thread
  // to the correct message loop. It also maintains some shared state between
  // all the contexts.
  class MessageFilter : public IPC::MessageFilter {
   public:
    MessageFilter();

    // Called on the IO thread.
    void AddRoute(int32_t route_id,
                  base::WeakPtr<IPC::Listener> listener,
                  scoped_refptr<base::SingleThreadTaskRunner> task_runner);
    // Called on the IO thread.
    void RemoveRoute(int32_t route_id);

    // IPC::MessageFilter implementation
    // (called on the IO thread):
    void OnFilterAdded(IPC::Channel* channel) override;
    bool OnMessageReceived(const IPC::Message& msg) override;
    void OnChannelError() override;
    void OnChannelClosing() override;

    void SendMessage(std::unique_ptr<IPC::Message> msg,
                     IPC::PendingSyncMsg* pending_sync);

    // The following methods can be called on any thread.

    // Whether the channel is lost.
    bool IsLost() const;

   private:
    struct ListenerInfo {
      ListenerInfo();
      ListenerInfo(const ListenerInfo& other);
      ~ListenerInfo();

      base::WeakPtr<IPC::Listener> listener;
      scoped_refptr<base::SingleThreadTaskRunner> task_runner;
    };

    ~MessageFilter() override;

    // Threading notes: most fields are only accessed on the IO thread, except
    // for lost_ which is protected by |lock_|.
    base::hash_map<int32_t, ListenerInfo> listeners_;
    IPC::Channel* channel_ = nullptr;
    std::vector<std::unique_ptr<IPC::Message>> pending_messages_;
    base::flat_map<int, IPC::PendingSyncMsg*> pending_syncs_;

    // Protects all fields below this one.
    mutable base::Lock lock_;

    // Whether the channel has been lost.
    bool lost_ = false;
  };

  GpuChannelHost(GpuChannelHostFactory* factory,
                 int channel_id,
                 const gpu::GPUInfo& gpu_info,
                 const gpu::GpuFeatureInfo& gpu_feature_info,
                 gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);
  ~GpuChannelHost() override;
  void Connect(const IPC::ChannelHandle& channel_handle,
               base::WaitableEvent* shutdown_event);
  bool InternalSend(IPC::Message* msg);
  void InternalFlush(uint32_t flush_id);

  // Threading notes: all fields are constant during the lifetime of |this|
  // except:
  // - |next_image_id_|, atomic type
  // - |next_route_id_|, atomic type
  // - |channel_| and |flush_list_|, protected by |context_lock_|
  GpuChannelHostFactory* const factory_;

  const int channel_id_;
  const gpu::GPUInfo gpu_info_;
  const gpu::GpuFeatureInfo gpu_feature_info_;

  scoped_refptr<MessageFilter> channel_filter_;

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;

  // Image IDs are allocated in sequence.
  base::AtomicSequenceNumber next_image_id_;

  // Route IDs are allocated in sequence.
  base::AtomicSequenceNumber next_route_id_;

  // Protects channel_ and flush_list_.
  mutable base::Lock context_lock_;
  std::unique_ptr<IPC::SyncChannel> channel_;
  std::vector<FlushParams> flush_list_;
  uint32_t next_flush_id_ = 1;
  uint32_t flushed_flush_id_ = 0;
  uint32_t verified_flush_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelHost);
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_GPU_CHANNEL_HOST_H_
