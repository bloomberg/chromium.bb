// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/ash_gpu_interface_provider.h"

#include "base/bind.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/ws/gpu_host/gpu_host.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ash {

AshGpuInterfaceProvider::AshGpuInterfaceProvider(
    ws::gpu_host::GpuHost* gpu_host,
    discardable_memory::DiscardableSharedMemoryManager*
        discardable_shared_memory_manager)
    : gpu_host_(gpu_host),
      discardable_shared_memory_manager_(discardable_shared_memory_manager) {
  DCHECK(gpu_host_);
  DCHECK(discardable_shared_memory_manager_);
}

AshGpuInterfaceProvider::~AshGpuInterfaceProvider() = default;

void AshGpuInterfaceProvider::RegisterGpuInterfaces(
    service_manager::BinderRegistry* registry) {
  registry->AddInterface<ws::mojom::ArcGpu>(base::BindRepeating(
      &AshGpuInterfaceProvider::BindArcGpuRequest, base::Unretained(this)));
  registry->AddInterface(base::BindRepeating(
      &AshGpuInterfaceProvider::BindDiscardableSharedMemoryManagerRequest,
      base::Unretained(this)));
  registry->AddInterface(base::BindRepeating(
      &AshGpuInterfaceProvider::BindGpuRequest, base::Unretained(this)));
}

void AshGpuInterfaceProvider::BindOzoneGpuInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle handle) {
  gpu_host_->BindOzoneGpuInterface(interface_name, std::move(handle));
}

void AshGpuInterfaceProvider::BindArcGpuRequest(
    ws::mojom::ArcGpuRequest request) {
  gpu_host_->AddArcGpu(std::move(request));
}

void AshGpuInterfaceProvider::BindDiscardableSharedMemoryManagerRequest(
    discardable_memory::mojom::DiscardableSharedMemoryManagerRequest request) {
  discardable_shared_memory_manager_->Bind(std::move(request),
                                           service_manager::BindSourceInfo());
}

void AshGpuInterfaceProvider::BindGpuRequest(ws::mojom::GpuRequest request) {
  gpu_host_->Add(std::move(request));
}

}  // namespace ash
