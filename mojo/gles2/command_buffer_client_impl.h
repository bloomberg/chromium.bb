// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GLES2_COMMAND_BUFFER_CLIENT_IMPL_H_
#define MOJO_GLES2_COMMAND_BUFFER_CLIENT_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/command_buffer.mojom.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class RunLoop;
}

namespace gles2 {
class CommandBufferClientImpl;

class CommandBufferDelegate {
 public:
  virtual ~CommandBufferDelegate();
  virtual void ContextLost();
};

class CommandBufferClientImpl
    : public mus::mojom::CommandBufferClient,
      public gpu::CommandBuffer,
      public gpu::GpuControl {
 public:
  explicit CommandBufferClientImpl(
      CommandBufferDelegate* delegate,
      const std::vector<int32_t>& attribs,
      const MojoAsyncWaiter* async_waiter,
      mojo::ScopedMessagePipeHandle command_buffer_handle);
  ~CommandBufferClientImpl() override;

  // CommandBuffer implementation:
  bool Initialize() override;
  State GetLastState() override;
  int32_t GetLastToken() override;
  void Flush(int32_t put_offset) override;
  void OrderingBarrier(int32_t put_offset) override;
  void WaitForTokenInRange(int32_t start, int32_t end) override;
  void WaitForGetOffsetInRange(int32_t start, int32_t end) override;
  void SetGetBuffer(int32_t shm_id) override;
  scoped_refptr<gpu::Buffer> CreateTransferBuffer(size_t size,
                                                  int32_t* id) override;
  void DestroyTransferBuffer(int32_t id) override;

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
  void SignalQuery(uint32_t query, const base::Closure& callback) override;
  void SetLock(base::Lock*) override;
  bool IsGpuChannelLost() override;
  void EnsureWorkVisible() override;
  gpu::CommandBufferNamespace GetNamespaceID() const override;
  uint64_t GetCommandBufferID() const override;
  int32_t GetExtraCommandBufferData() const override;
  uint64_t GenerateFenceSyncRelease() override;
  bool IsFenceSyncRelease(uint64_t release) override;
  bool IsFenceSyncFlushed(uint64_t release) override;
  bool IsFenceSyncFlushReceived(uint64_t release) override;
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       const base::Closure& callback) override;
  bool CanWaitUnverifiedSyncToken(const gpu::SyncToken* sync_token) override;

 private:
  // mus::mojom::CommandBufferClient implementation:
  void Destroyed(int32_t lost_reason, int32_t error) override;
  void SignalAck(uint32_t id) override;
  void SwapBuffersCompleted(int32_t result) override;
  void UpdateState(const gpu::CommandBuffer::State& state) override;
  void UpdateVSyncParameters(int64_t timebase, int64_t interval) override;

  void TryUpdateState();
  void MakeProgressAndUpdateState();

  gpu::CommandBufferSharedState* shared_state() const { return shared_state_; }

  CommandBufferDelegate* delegate_;
  std::vector<int32_t> attribs_;
  mojo::Binding<mus::mojom::CommandBufferClient> client_binding_;
  mus::mojom::CommandBufferPtr command_buffer_;

  uint64_t command_buffer_id_;
  gpu::Capabilities capabilities_;
  State last_state_;
  mojo::ScopedSharedBufferHandle shared_state_handle_;
  gpu::CommandBufferSharedState* shared_state_;
  int32_t last_put_offset_;
  int32_t next_transfer_buffer_id_;

  // Image IDs are allocated in sequence.
  int next_image_id_;

  uint64_t next_fence_sync_release_;
  uint64_t flushed_fence_sync_release_;

  const MojoAsyncWaiter* async_waiter_;
};

}  // gles2

#endif  // MOJO_GLES2_COMMAND_BUFFER_CLIENT_IMPL_H_
