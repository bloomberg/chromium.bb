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
#include "mojo/services/public/interfaces/gpu/command_buffer.mojom.h"

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
  ~CommandBufferClientImpl() override;

  // CommandBuffer implementation:
  bool Initialize() override;
  State GetLastState() override;
  int32 GetLastToken() override;
  void Flush(int32 put_offset) override;
  void WaitForTokenInRange(int32 start, int32 end) override;
  void WaitForGetOffsetInRange(int32 start, int32 end) override;
  void SetGetBuffer(int32 shm_id) override;
  scoped_refptr<gpu::Buffer> CreateTransferBuffer(size_t size,
                                                  int32* id) override;
  void DestroyTransferBuffer(int32 id) override;

  // gpu::GpuControl implementation:
  gpu::Capabilities GetCapabilities() override;
  int32_t CreateImage(ClientBuffer buffer,
                      size_t width,
                      size_t height,
                      unsigned internalformat) override;
  void DestroyImage(int32_t id) override;
  int32_t CreateGpuMemoryBufferImage(size_t width,
                                     size_t height,
                                     unsigned internalformat,
                                     unsigned usage) override;
  uint32 InsertSyncPoint() override;
  uint32 InsertFutureSyncPoint() override;
  void RetireSyncPoint(uint32 sync_point) override;
  void SignalSyncPoint(uint32 sync_point,
                       const base::Closure& callback) override;
  void SignalQuery(uint32 query, const base::Closure& callback) override;
  void SetSurfaceVisible(bool visible) override;
  uint32 CreateStreamTexture(uint32 texture_id) override;

 private:
  class SyncClientImpl;

  // CommandBufferClient implementation:
  void DidDestroy() override;
  void LostContext(int32_t lost_reason) override;

  // ErrorHandler implementation:
  void OnConnectionError() override;

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
