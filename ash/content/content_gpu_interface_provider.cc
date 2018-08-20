// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/content/content_gpu_interface_provider.h"

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/ref_counted.h"
#include "components/discardable_memory/public/interfaces/discardable_shared_memory_manager.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_client.h"
#include "content/public/browser/gpu_service_registry.h"
#include "services/ui/public/interfaces/gpu.mojom.h"

namespace ash {

// InterfaceBinderImpl handles the actual binding. The binding has to happen on
// the IO thread.
class ContentGpuInterfaceProvider::InterfaceBinderImpl
    : public base::RefCountedThreadSafe<InterfaceBinderImpl> {
 public:
  InterfaceBinderImpl() = default;

  void BindGpuRequestOnGpuTaskRunner(ui::mojom::GpuRequest request) {
    // The GPU task runner is bound to the IO thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    auto gpu_client = content::CreateGpuClient(
        std::move(request),
        base::BindOnce(&InterfaceBinderImpl::OnGpuClientConnectionError, this),
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::IO));
    gpu_clients_.push_back(std::move(gpu_client));
  }

  void BindDiscardableSharedMemoryManagerOnGpuTaskRunner(
      discardable_memory::mojom::DiscardableSharedMemoryManagerRequest
          request) {
    content::BindInterfaceInGpuProcess(std::move(request));
  }

 private:
  friend class base::RefCountedThreadSafe<InterfaceBinderImpl>;
  ~InterfaceBinderImpl() = default;

  void OnGpuClientConnectionError(viz::GpuClient* client) {
    base::EraseIf(
        gpu_clients_,
        base::UniquePtrMatcher<viz::GpuClient, base::OnTaskRunnerDeleter>(
            client));
  }

  std::vector<std::unique_ptr<viz::GpuClient, base::OnTaskRunnerDeleter>>
      gpu_clients_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceBinderImpl);
};

ContentGpuInterfaceProvider::ContentGpuInterfaceProvider()
    : interface_binder_impl_(base::MakeRefCounted<InterfaceBinderImpl>()) {}

ContentGpuInterfaceProvider::~ContentGpuInterfaceProvider() = default;

void ContentGpuInterfaceProvider::RegisterGpuInterfaces(
    service_manager::BinderRegistry* registry) {
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner =
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::IO);
  registry->AddInterface(
      base::BindRepeating(&InterfaceBinderImpl::
                              BindDiscardableSharedMemoryManagerOnGpuTaskRunner,
                          interface_binder_impl_),
      gpu_task_runner);
  registry->AddInterface(
      base::BindRepeating(&InterfaceBinderImpl::BindGpuRequestOnGpuTaskRunner,
                          interface_binder_impl_),
      gpu_task_runner);
}

void ContentGpuInterfaceProvider::RegisterOzoneGpuInterfaces(
    service_manager::BinderRegistry* registry) {
  // Registers the gpu-related interfaces needed by Ozone.
  // TODO(rjkroege): Adjust when Ozone/DRM/Mojo is complete.
  NOTIMPLEMENTED();
}

}  // namespace ash
