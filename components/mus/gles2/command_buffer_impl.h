// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_COMMAND_BUFFER_IMPL_H_
#define COMPONENTS_MUS_GLES2_COMMAND_BUFFER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/mus/public/interfaces/command_buffer.mojom.h"
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
                    scoped_refptr<GpuState> gpu_state,
                    scoped_ptr<CommandBufferDriver> driver);
  void DidLoseContext();

  void set_observer(CommandBufferImplObserver* observer) {
    observer_ = observer;
  }

 private:
  class CommandBufferDriverClientImpl;
  ~CommandBufferImpl() override;

  // mojom::CommandBuffer:
  void Initialize(mojom::CommandBufferSyncClientPtr sync_client,
                  mojom::CommandBufferSyncPointClientPtr sync_point_client,
                  mojom::CommandBufferLostContextObserverPtr loss_observer,
                  mojo::ScopedSharedBufferHandle shared_state,
                  mojo::Array<int32_t> attribs) override;
  void SetGetBuffer(int32_t buffer) override;
  void Flush(int32_t put_offset) override;
  void MakeProgress(int32_t last_get_offset) override;
  void RegisterTransferBuffer(int32_t id,
                              mojo::ScopedSharedBufferHandle transfer_buffer,
                              uint32_t size) override;
  void DestroyTransferBuffer(int32_t id) override;
  void InsertSyncPoint(bool retire) override;
  void RetireSyncPoint(uint32_t sync_point) override;
  void Echo(const mojo::Callback<void()>& callback) override;
  void CreateImage(int32_t id,
                   mojo::ScopedHandle memory_handle,
                   int32_t type,
                   mojo::SizePtr size,
                   int32_t format,
                   int32_t internal_format) override;
  void DestroyImage(int32_t id) override;

  bool InitializeHelper(
      mojom::CommandBufferSyncClientPtr sync_client,
      mojom::CommandBufferLostContextObserverPtr loss_observer,
      mojo::ScopedSharedBufferHandle shared_state,
      mojo::Array<int32_t> attribs);
  bool SetGetBufferHelper(int32_t buffer);
  bool FlushHelper(int32_t put_offset, uint32_t order_num);
  bool MakeProgressHelper(int32_t last_get_offset);
  bool RegisterTransferBufferHelper(
      int32_t id,
      mojo::ScopedSharedBufferHandle transfer_buffer,
      uint32_t size);
  bool DestroyTransferBufferHelper(int32_t id);
  bool RetireSyncPointHelper(uint32_t sync_point);
  bool EchoHelper(
      const tracked_objects::Location& from_here,
      scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner,
      const base::Closure& reply);
  bool CreateImageHelper(
      int32_t id,
      mojo::ScopedHandle memory_handle,
      int32_t type,
      mojo::SizePtr size,
      int32_t format,
      int32_t internal_format);
  bool DestroyImageHelper(int32_t id);

  void BindToRequest(mojo::InterfaceRequest<CommandBuffer> request);

  void OnConnectionError();
  bool DeleteHelper();

  scoped_refptr<GpuState> gpu_state_;
  scoped_ptr<CommandBufferDriver> driver_;
  mojom::CommandBufferSyncPointClientPtr sync_point_client_;
  scoped_ptr<mojo::Binding<CommandBuffer>> binding_;
  CommandBufferImplObserver* observer_;
  base::WeakPtrFactory<CommandBufferImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferImpl);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_GLES2_COMMAND_BUFFER_IMPL_H_
