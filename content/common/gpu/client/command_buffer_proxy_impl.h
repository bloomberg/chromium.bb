// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_
#define CONTENT_COMMON_GPU_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_

#include <map>
#include <queue>
#include <string>

#include "gpu/ipc/command_buffer_proxy.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/common/gpu/client/gpu_video_decode_accelerator_host.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "ipc/ipc_listener.h"

struct GPUCommandBufferConsoleMessage;

namespace base {
class SharedMemory;
}

namespace content {
class GpuChannelHost;

// Client side proxy that forwards messages synchronously to a
// CommandBufferStub.
class CommandBufferProxyImpl
    : public CommandBufferProxy,
      public IPC::Listener,
      public base::SupportsWeakPtr<CommandBufferProxyImpl> {
 public:
  typedef base::Callback<void(
      const std::string& msg, int id)> GpuConsoleMessageCallback;

  CommandBufferProxyImpl(GpuChannelHost* channel, int route_id);
  virtual ~CommandBufferProxyImpl();

  // Sends an IPC message to create a GpuVideoDecodeAccelerator. Creates and
  // returns a pointer to a GpuVideoDecodeAcceleratorHost.
  // Returns NULL on failure to create the GpuVideoDecodeAcceleratorHost.
  // Note that the GpuVideoDecodeAccelerator may still fail to be created in
  // the GPU process, even if this returns non-NULL. In this case the client is
  // notified of an error later.
  GpuVideoDecodeAcceleratorHost* CreateVideoDecoder(
      media::VideoCodecProfile profile,
      media::VideoDecodeAccelerator::Client* client);

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // CommandBufferProxy implementation:
  virtual int GetRouteID() const OVERRIDE;
  virtual bool Echo(const base::Closure& callback) OVERRIDE;
  virtual bool SetParent(CommandBufferProxy* parent_command_buffer,
                         uint32 parent_texture_id) OVERRIDE;
  virtual void SetChannelErrorCallback(const base::Closure& callback) OVERRIDE;

  // CommandBuffer implementation:
  virtual bool Initialize() OVERRIDE;
  virtual State GetState() OVERRIDE;
  virtual State GetLastState() OVERRIDE;
  virtual void Flush(int32 put_offset) OVERRIDE;
  virtual State FlushSync(int32 put_offset, int32 last_known_get) OVERRIDE;
  virtual void SetGetBuffer(int32 shm_id) OVERRIDE;
  virtual void SetGetOffset(int32 get_offset) OVERRIDE;
  virtual gpu::Buffer CreateTransferBuffer(size_t size,
                                           int32* id) OVERRIDE;
  virtual void DestroyTransferBuffer(int32 id) OVERRIDE;
  virtual gpu::Buffer GetTransferBuffer(int32 id) OVERRIDE;
  virtual void SetToken(int32 token) OVERRIDE;
  virtual void SetParseError(gpu::error::Error error) OVERRIDE;
  virtual void SetContextLostReason(
      gpu::error::ContextLostReason reason) OVERRIDE;

  void SetMemoryAllocationChangedCallback(
      const base::Callback<void(const GpuMemoryAllocationForRenderer&)>&
          callback);

  bool DiscardBackbuffer();
  bool EnsureBackbuffer();

  // Inserts a sync point, returning its ID. This is handled on the IO thread of
  // the GPU process, and so should be relatively fast, but its effect is
  // ordered wrt other messages (in particular, Flush). Sync point IDs are
  // global and can be used for cross-channel synchronization.
  uint32 InsertSyncPoint();

  // Makes this command buffer wait on a sync point. This command buffer will be
  // unscheduled until the command buffer that inserted that sync point reaches
  // it, or gets destroyed.
  void WaitSyncPoint(uint32);

  // Makes this command buffer invoke a task when a sync point is reached, or
  // the command buffer that inserted that sync point is destroyed.
  bool SignalSyncPoint(uint32 sync_point,
                       const base::Closure& callback);

  // Generates n unique mailbox names that can be used with
  // GL_texture_mailbox_CHROMIUM. Unlike genMailboxCHROMIUM, this IPC is
  // handled only on the GPU process' IO thread, and so is not effectively
  // a finish.
  bool GenerateMailboxNames(unsigned num, std::vector<std::string>* names);

  // Sends an IPC message with the new state of surface visibility.
  bool SetSurfaceVisible(bool visible);

  void SetOnConsoleMessageCallback(
      const GpuConsoleMessageCallback& callback);

  // TODO(apatrick): this is a temporary optimization while skia is calling
  // ContentGLContext::MakeCurrent prior to every GL call. It saves returning 6
  // ints redundantly when only the error is needed for the
  // CommandBufferProxyImpl implementation.
  virtual gpu::error::Error GetLastError() OVERRIDE;

  void SendManagedMemoryStats(const GpuManagedMemoryStats& stats);

  GpuChannelHost* channel() const { return channel_; }

 private:
  typedef std::map<int32, gpu::Buffer> TransferBufferMap;
  typedef std::map<int, base::WeakPtr<GpuVideoDecodeAcceleratorHost> > Decoders;
  typedef base::hash_map<uint32, base::Closure> SignalTaskMap;

  // Send an IPC message over the GPU channel. This is private to fully
  // encapsulate the channel; all callers of this function must explicitly
  // verify that the context has not been lost.
  bool Send(IPC::Message* msg);

  // Message handlers:
  void OnUpdateState(const gpu::CommandBuffer::State& state);
  void OnDestroyed(gpu::error::ContextLostReason reason);
  void OnEchoAck();
  void OnConsoleMessage(const GPUCommandBufferConsoleMessage& message);
  void OnSetMemoryAllocation(const GpuMemoryAllocationForRenderer& allocation);
  void OnSignalSyncPointAck(uint32 id);
  void OnGenerateMailboxNamesReply(const std::vector<std::string>& names);

  // Try to read an updated copy of the state from shared memory.
  void TryUpdateState();

  // The shared memory area used to update state.
  gpu::CommandBufferSharedState* shared_state() const {
    return reinterpret_cast<gpu::CommandBufferSharedState*>(
        shared_state_shm_->memory());
  }

  // Local cache of id to transfer buffer mapping.
  TransferBufferMap transfer_buffers_;

  // Zero or more (unowned!) video decoder hosts using this proxy, keyed by
  // their decoder_route_id.
  Decoders video_decoder_hosts_;

  // The last cached state received from the service.
  State last_state_;

  // The shared memory area used to update state.
  scoped_ptr<base::SharedMemory> shared_state_shm_;

  // |*this| is owned by |*channel_| and so is always outlived by it, so using a
  // raw pointer is ok.
  GpuChannelHost* channel_;
  int route_id_;
  unsigned int flush_count_;
  int32 last_put_offset_;

  // Tasks to be invoked in echo responses.
  std::queue<base::Closure> echo_tasks_;

  base::Closure channel_error_callback_;

  base::Callback<void(const GpuMemoryAllocationForRenderer&)>
      memory_allocation_changed_callback_;

  GpuConsoleMessageCallback console_message_callback_;

  // Tasks to be invoked in SignalSyncPoint responses.
  uint32 next_signal_id_;
  SignalTaskMap signal_tasks_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferProxyImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_
