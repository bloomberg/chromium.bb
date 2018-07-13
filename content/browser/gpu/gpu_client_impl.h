// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_CLIENT_IMPL_H_
#define CONTENT_BROWSER_GPU_GPU_CLIENT_IMPL_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/gpu_client.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace content {

class GpuClientImpl : public ui::mojom::GpuMemoryBufferFactory,
                      public ui::mojom::Gpu,
                      public GpuClient {
 public:
  // GpuClientImpl must be destroyed on the thread associated with
  // |task_runner|.
  GpuClientImpl(int client_id,
                uint64_t client_tracing_id,
                scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  ~GpuClientImpl() override;

  // This needs to be run on the thread associated with |task_runner_|.
  void Add(ui::mojom::GpuRequest request);

  void PreEstablishGpuChannel();

  void SetConnectionErrorHandler(
      ConnectionErrorHandlerClosure connection_error_handler);

  // ui::mojom::GpuMemoryBufferFactory overrides:
  void CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      ui::mojom::GpuMemoryBufferFactory::CreateGpuMemoryBufferCallback callback)
      override;
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              const gpu::SyncToken& sync_token) override;

  // ui::mojom::Gpu overrides:
  void CreateGpuMemoryBufferFactory(
      ui::mojom::GpuMemoryBufferFactoryRequest request) override;
  void EstablishGpuChannel(EstablishGpuChannelCallback callback) override;
  void CreateJpegDecodeAccelerator(
      media::mojom::JpegDecodeAcceleratorRequest jda_request) override;
  void CreateVideoEncodeAcceleratorProvider(
      media::mojom::VideoEncodeAcceleratorProviderRequest vea_provider_request)
      override;

 private:
  enum class ErrorReason {
    // OnError() is being called from the destructor.
    kInDestructor,
    // OnError() is being called because the connection was lost.
    kConnectionLost
  };
  void OnError(ErrorReason reason);
  void OnEstablishGpuChannel(mojo::ScopedMessagePipeHandle channel_handle,
                             const gpu::GPUInfo& gpu_info,
                             const gpu::GpuFeatureInfo& gpu_feature_info,
                             GpuProcessHost::EstablishChannelStatus status);
  void OnCreateGpuMemoryBuffer(CreateGpuMemoryBufferCallback callback,
                               gfx::GpuMemoryBufferHandle handle);
  void ClearCallback();

  const int client_id_;
  const uint64_t client_tracing_id_;
  mojo::BindingSet<ui::mojom::GpuMemoryBufferFactory>
      gpu_memory_buffer_factory_bindings_;
  mojo::BindingSet<ui::mojom::Gpu> gpu_bindings_;
  bool gpu_channel_requested_ = false;
  EstablishGpuChannelCallback callback_;
  mojo::ScopedMessagePipeHandle channel_handle_;
  gpu::GPUInfo gpu_info_;
  gpu::GpuFeatureInfo gpu_feature_info_;
  ConnectionErrorHandlerClosure connection_error_handler_;
  // |task_runner_| is associated with the thread |gpu_bindings_| is bound on.
  // GpuClientImpl instance is bound to this thread, and must be destroyed on
  // this thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<GpuClientImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuClientImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_CLIENT_IMPL_H_
