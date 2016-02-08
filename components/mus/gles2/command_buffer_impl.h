// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_COMMAND_BUFFER_IMPL_H_
#define COMPONENTS_MUS_GLES2_COMMAND_BUFFER_IMPL_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/mus/public/interfaces/command_buffer.mojom.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mus {

class CommandBufferDriver;
class CommandBufferImplObserver;
class GpuState;

// This class listens to the CommandBuffer message pipe on a low-latency thread
// so that we can insert sync points without blocking on the GL driver. It
// forwards most method calls to the CommandBufferDriver, which runs on the
// same thread as the native viewport.
class CommandBufferImpl : public mojom::CommandBuffer {
 public:
  CommandBufferImpl(mojo::InterfaceRequest<CommandBuffer> request,
                    scoped_refptr<GpuState> gpu_state);
  void DidLoseContext(uint32_t reason);

  void set_observer(CommandBufferImplObserver* observer) {
    observer_ = observer;
  }

 private:
  class CommandBufferDriverClientImpl;
  ~CommandBufferImpl() override;

  // mojom::CommandBuffer:
  void Initialize(
      mojom::CommandBufferLostContextObserverPtr loss_observer,
      mojo::ScopedSharedBufferHandle shared_state,
      mojo::Array<int32_t> attribs,
      const mojom::CommandBuffer::InitializeCallback& callback) override;
  void SetGetBuffer(int32_t buffer) override;
  void Flush(int32_t put_offset) override;
  void MakeProgress(
      int32_t last_get_offset,
      const mojom::CommandBuffer::MakeProgressCallback& callback) override;
  void RegisterTransferBuffer(int32_t id,
                              mojo::ScopedSharedBufferHandle transfer_buffer,
                              uint32_t size) override;
  void DestroyTransferBuffer(int32_t id) override;
  void CreateImage(int32_t id,
                   mojo::ScopedHandle memory_handle,
                   int32_t type,
                   mojo::SizePtr size,
                   int32_t format,
                   int32_t internal_format) override;
  void DestroyImage(int32_t id) override;

  // All helper functions are called in the GPU therad.
  void InitializeOnGpuThread(
      mojom::CommandBufferLostContextObserverPtr loss_observer,
      mojo::ScopedSharedBufferHandle shared_state,
      mojo::Array<int32_t> attribs,
      const base::Callback<
          void(mojom::CommandBufferInitializeResultPtr)>& callback);
  bool SetGetBufferOnGpuThread(int32_t buffer);
  bool FlushOnGpuThread(int32_t put_offset, uint32_t order_num);
  bool MakeProgressOnGpuThread(
      int32_t last_get_offset,
      const base::Callback<void(const gpu::CommandBuffer::State&)>& callback);
  bool RegisterTransferBufferOnGpuThread(
      int32_t id,
      mojo::ScopedSharedBufferHandle transfer_buffer,
      uint32_t size);
  bool DestroyTransferBufferOnGpuThread(int32_t id);
  bool CreateImageOnGpuThread(int32_t id,
                              mojo::ScopedHandle memory_handle,
                              int32_t type,
                              mojo::SizePtr size,
                              int32_t format,
                              int32_t internal_format);
  bool DestroyImageOnGpuThread(int32_t id);

  void BindToRequest(mojo::InterfaceRequest<CommandBuffer> request);

  void OnConnectionError();
  bool DeleteOnGpuThread();

  scoped_refptr<GpuState> gpu_state_;
  scoped_ptr<CommandBufferDriver> driver_;
  scoped_ptr<mojo::Binding<CommandBuffer>> binding_;
  CommandBufferImplObserver* observer_;
  mojom::CommandBufferLostContextObserverPtr loss_observer_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferImpl);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_GLES2_COMMAND_BUFFER_IMPL_H_
