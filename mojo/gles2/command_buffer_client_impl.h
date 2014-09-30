// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GLES2_COMMAND_BUFFER_CLIENT_IMPL_H_
#define MOJO_GLES2_COMMAND_BUFFER_CLIENT_IMPL_H_

#include <map>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/services/gles2/command_buffer.mojom.h"

namespace base {
class RunLoop;
}

namespace mojo {
namespace gles2 {
class CommandBufferClientImpl;

class CommandBufferDelegate {
 public:
  virtual ~CommandBufferDelegate();
  virtual void ContextLost();
};

class CommandBufferClientImpl : public CommandBufferClient,
                                public ErrorHandler,
                                public gpu::CommandBuffer,
                                public gpu::GpuControl {
 public:
  explicit CommandBufferClientImpl(
      CommandBufferDelegate* delegate,
      const MojoAsyncWaiter* async_waiter,
      ScopedMessagePipeHandle command_buffer_handle);
  virtual ~CommandBufferClientImpl();

  // CommandBuffer implementation:
  virtual bool Initialize() override;
  virtual State GetLastState() override;
  virtual int32 GetLastToken() override;
  virtual void Flush(int32 put_offset) override;
  virtual void WaitForTokenInRange(int32 start, int32 end) override;
  virtual void WaitForGetOffsetInRange(int32 start, int32 end) override;
  virtual void SetGetBuffer(int32 shm_id) override;
  virtual scoped_refptr<gpu::Buffer> CreateTransferBuffer(size_t size,
                                                          int32* id) override;
  virtual void DestroyTransferBuffer(int32 id) override;

  // gpu::GpuControl implementation:
  virtual gpu::Capabilities GetCapabilities() override;
  virtual gfx::GpuMemoryBuffer* CreateGpuMemoryBuffer(size_t width,
                                                      size_t height,
                                                      unsigned internalformat,
                                                      unsigned usage,
                                                      int32* id) override;
  virtual void DestroyGpuMemoryBuffer(int32 id) override;
  virtual uint32 InsertSyncPoint() override;
  virtual uint32 InsertFutureSyncPoint() override;
  virtual void RetireSyncPoint(uint32 sync_point) override;
  virtual void SignalSyncPoint(uint32 sync_point,
                               const base::Closure& callback) override;
  virtual void SignalQuery(uint32 query,
                           const base::Closure& callback) override;
  virtual void SetSurfaceVisible(bool visible) override;
  virtual void Echo(const base::Closure& callback) override;
  virtual uint32 CreateStreamTexture(uint32 texture_id) override;

 private:
  class SyncClientImpl;

  // CommandBufferClient implementation:
  virtual void DidDestroy() override;
  virtual void LostContext(int32_t lost_reason) override;

  // ErrorHandler implementation:
  virtual void OnConnectionError() override;

  void TryUpdateState();
  void MakeProgressAndUpdateState();

  gpu::CommandBufferSharedState* shared_state() const { return shared_state_; }

  CommandBufferDelegate* delegate_;
  CommandBufferPtr command_buffer_;
  scoped_ptr<SyncClientImpl> sync_client_impl_;

  State last_state_;
  mojo::ScopedSharedBufferHandle shared_state_handle_;
  gpu::CommandBufferSharedState* shared_state_;
  int32 last_put_offset_;
  int32 next_transfer_buffer_id_;

  const MojoAsyncWaiter* async_waiter_;
};

}  // gles2
}  // mojo

#endif  // MOJO_GLES2_COMMAND_BUFFER_CLIENT_IMPL_H_
