// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GLES2_COMMAND_BUFFER_CLIENT_IMPL_H_
#define MOJO_GLES2_COMMAND_BUFFER_CLIENT_IMPL_H_

#include <map>
#include <queue>

#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "gpu/command_buffer/common/gpu_control.h"

#include "mojo/public/bindings/error_handler.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojom/command_buffer.h"

namespace base {
class RunLoop;
}

namespace mojo {
template <typename S>
class SyncDispatcher;

namespace gles2 {
class CommandBufferClientImpl;

class CommandBufferDelegate {
 public:
  virtual ~CommandBufferDelegate();
  virtual void ContextLost();
  virtual void DrawAnimationFrame();
};

class CommandBufferClientImpl : public CommandBufferClient,
                                public CommandBufferSyncClient,
                                public ErrorHandler,
                                public gpu::CommandBuffer,
                                public gpu::GpuControl {
 public:
  explicit CommandBufferClientImpl(
      CommandBufferDelegate* delegate,
      MojoAsyncWaiter* async_waiter,
      ScopedCommandBufferHandle command_buffer_handle);
  virtual ~CommandBufferClientImpl();

  // CommandBuffer implementation:
  virtual bool Initialize() OVERRIDE;
  virtual State GetState() OVERRIDE;
  virtual State GetLastState() OVERRIDE;
  virtual int32 GetLastToken() OVERRIDE;
  virtual void Flush(int32 put_offset) OVERRIDE;
  virtual State FlushSync(int32 put_offset, int32 last_known_get) OVERRIDE;
  virtual void SetGetBuffer(int32 shm_id) OVERRIDE;
  virtual void SetGetOffset(int32 get_offset) OVERRIDE;
  virtual gpu::Buffer CreateTransferBuffer(size_t size, int32* id) OVERRIDE;
  virtual void DestroyTransferBuffer(int32 id) OVERRIDE;
  virtual gpu::Buffer GetTransferBuffer(int32 id) OVERRIDE;
  virtual void SetToken(int32 token) OVERRIDE;
  virtual void SetParseError(gpu::error::Error error) OVERRIDE;
  virtual void SetContextLostReason(gpu::error::ContextLostReason reason)
      OVERRIDE;

  // gpu::GpuControl implementation:
  virtual gpu::Capabilities GetCapabilities() OVERRIDE;
  virtual gfx::GpuMemoryBuffer* CreateGpuMemoryBuffer(size_t width,
                                                      size_t height,
                                                      unsigned internalformat,
                                                      int32* id) OVERRIDE;
  virtual void DestroyGpuMemoryBuffer(int32 id) OVERRIDE;
  virtual bool GenerateMailboxNames(unsigned num,
                                    std::vector<gpu::Mailbox>* names) OVERRIDE;
  virtual uint32 InsertSyncPoint() OVERRIDE;
  virtual void SignalSyncPoint(uint32 sync_point,
                               const base::Closure& callback) OVERRIDE;
  virtual void SignalQuery(uint32 query,
                           const base::Closure& callback) OVERRIDE;
  virtual void SetSurfaceVisible(bool visible) OVERRIDE;
  virtual void SendManagedMemoryStats(const gpu::ManagedMemoryStats& stats)
      OVERRIDE;
  virtual void Echo(const base::Closure& callback) OVERRIDE;
  virtual uint32 CreateStreamTexture(uint32 texture_id) OVERRIDE;

  void RequestAnimationFrames();
  void CancelAnimationFrames();

 private:
  typedef std::map<int32, gpu::Buffer> TransferBufferMap;

  // CommandBufferClient implementation:
  virtual void DidInitialize(bool success) MOJO_OVERRIDE;
  virtual void DidMakeProgress(const CommandBufferState& state) MOJO_OVERRIDE;
  virtual void DidDestroy() MOJO_OVERRIDE;
  virtual void EchoAck() MOJO_OVERRIDE;
  virtual void LostContext(int32_t lost_reason) MOJO_OVERRIDE;

  // ErrorHandler implementation:
  virtual void OnError() MOJO_OVERRIDE;

  virtual void DrawAnimationFrame() MOJO_OVERRIDE;

  void TryUpdateState();
  void MakeProgressAndUpdateState();

  gpu::CommandBufferSharedState* shared_state() const {
    return reinterpret_cast<gpu::CommandBufferSharedState*>(
        shared_state_shm_->memory());
  }

  CommandBufferDelegate* delegate_;
  RemotePtr<mojo::CommandBuffer> command_buffer_;
  scoped_ptr<SyncDispatcher<CommandBufferSyncClient> > sync_dispatcher_;

  State last_state_;
  scoped_ptr<base::SharedMemory> shared_state_shm_;
  TransferBufferMap transfer_buffers_;
  int32 last_put_offset_;
  int32 next_transfer_buffer_id_;
  std::queue<base::Closure> echo_closures_;

  bool initialize_result_;
};

}  // gles2
}  // mojo

#endif  // MOJO_GLES2_COMMAND_BUFFER_CLIENT_IMPL_H_
