// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_CLIENT_H_
#define CONTENT_BROWSER_GPU_GPU_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/ui/public/interfaces/gpu.mojom.h"

namespace content {

class GpuClient : public ui::mojom::Gpu {
 public:
  explicit GpuClient(int render_process_id);
  ~GpuClient() override;

  void Add(ui::mojom::GpuRequest request);

  void PreEstablishGpuChannel();

 private:
  void OnError();
  void OnEstablishGpuChannel(mojo::ScopedMessagePipeHandle channel_handle,
                             const gpu::GPUInfo& gpu_info,
                             const gpu::GpuFeatureInfo& gpu_feature_info,
                             GpuProcessHost::EstablishChannelStatus status);
  void OnCreateGpuMemoryBuffer(const CreateGpuMemoryBufferCallback& callback,
                               const gfx::GpuMemoryBufferHandle& handle);
  void ClearCallback();

  // ui::mojom::Gpu overrides:
  void EstablishGpuChannel(
      const EstablishGpuChannelCallback& callback) override;
  void CreateJpegDecodeAccelerator(
      media::mojom::JpegDecodeAcceleratorRequest jda_request) override;
  void CreateVideoEncodeAcceleratorProvider(
      media::mojom::VideoEncodeAcceleratorProviderRequest vea_provider_request)
      override;
  void CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      const ui::mojom::Gpu::CreateGpuMemoryBufferCallback& callback) override;
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              const gpu::SyncToken& sync_token) override;

  const int render_process_id_;
  mojo::BindingSet<ui::mojom::Gpu> bindings_;
  bool gpu_channel_requested_ = false;
  EstablishGpuChannelCallback callback_;
  mojo::ScopedMessagePipeHandle channel_handle_;
  gpu::GPUInfo gpu_info_;
  gpu::GpuFeatureInfo gpu_feature_info_;
  base::WeakPtrFactory<GpuClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_CLIENT_H_
