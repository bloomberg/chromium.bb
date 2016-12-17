// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_CLIENT_H_
#define CONTENT_BROWSER_GPU_GPU_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "ipc/ipc_channel_handle.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/ui/public/interfaces/gpu.mojom.h"

namespace content {

class GpuClient : public ui::mojom::Gpu {
 public:
  explicit GpuClient(int render_process_id);
  ~GpuClient() override;

  void Add(ui::mojom::GpuRequest request);

 private:
  void OnError();
  void OnEstablishGpuChannel(const EstablishGpuChannelCallback& callback,
                             const IPC::ChannelHandle& channel,
                             const gpu::GPUInfo& gpu_info);
  void OnCreateGpuMemoryBuffer(const CreateGpuMemoryBufferCallback& callback,
                               const gfx::GpuMemoryBufferHandle& handle);

  // ui::mojom::Gpu overrides:
  void EstablishGpuChannel(
      const EstablishGpuChannelCallback& callback) override;
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
  base::WeakPtrFactory<GpuClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_CLIENT_H_
