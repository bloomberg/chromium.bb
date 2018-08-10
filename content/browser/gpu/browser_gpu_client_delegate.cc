// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/browser_gpu_client_delegate.h"

#include "content/browser/gpu/gpu_process_host.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace content {
namespace {

void OnEstablishGpuChannel(
    GpuClientDelegate::EstablishGpuChannelCallback callback,
    mojo::ScopedMessagePipeHandle channel_handle,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    GpuProcessHost::EstablishChannelStatus status) {
  if (!callback)
    return;

  GpuClientDelegate::EstablishGpuChannelStatus delegate_status;
  switch (status) {
    case GpuProcessHost::EstablishChannelStatus::GPU_ACCESS_DENIED:
      delegate_status =
          GpuClientDelegate::EstablishGpuChannelStatus::kGpuAccessDenied;
      break;
    case GpuProcessHost::EstablishChannelStatus::GPU_HOST_INVALID:
      delegate_status =
          GpuClientDelegate::EstablishGpuChannelStatus::kGpuHostInvalid;
      break;
    case GpuProcessHost::EstablishChannelStatus::SUCCESS:
      delegate_status = GpuClientDelegate::EstablishGpuChannelStatus::kSuccess;
      break;
  }
  std::move(callback).Run(std::move(channel_handle), gpu_info, gpu_feature_info,
                          delegate_status);
}

}  // namespace

BrowserGpuClientDelegate::BrowserGpuClientDelegate() = default;

BrowserGpuClientDelegate::~BrowserGpuClientDelegate() = default;

viz::mojom::GpuService* BrowserGpuClientDelegate::EnsureGpuService() {
  if (auto* host = GpuProcessHost::Get())
    return host->gpu_service();
  return nullptr;
}

void BrowserGpuClientDelegate::EstablishGpuChannel(
    int client_id,
    uint64_t client_tracing_id,
    EstablishGpuChannelCallback callback) {
  auto* host = GpuProcessHost::Get();
  if (!host) {
    std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            EstablishGpuChannelStatus::kGpuAccessDenied);
    return;
  }

  const bool is_gpu_host = false;
  host->EstablishGpuChannel(
      client_id, client_tracing_id, is_gpu_host,
      base::BindOnce(&OnEstablishGpuChannel, std::move(callback)));
}

}  // namespace content
