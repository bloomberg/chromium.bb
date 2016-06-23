// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/common/gpu_service.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/mus/common/gpu_type_converters.h"
#include "components/mus/common/mojo_gpu_memory_buffer_manager.h"
#include "components/mus/common/switches.h"
#include "components/mus/public/interfaces/gpu_service.mojom.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/shell/public/cpp/connector.h"

namespace mus {

GpuService::GpuService()
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      shutdown_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
      io_thread_("GPUIOThread"),
      gpu_memory_buffer_manager_(new MojoGpuMemoryBufferManager) {
  base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
  thread_options.priority = base::ThreadPriority::NORMAL;
  CHECK(io_thread_.StartWithOptions(thread_options));
}

GpuService::~GpuService() {}

// static
bool GpuService::UseChromeGpuCommandBuffer() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseChromeGpuCommandBufferInMus);
}

// static
GpuService* GpuService::GetInstance() {
  return base::Singleton<GpuService,
                         base::LeakySingletonTraits<GpuService>>::get();
}

scoped_refptr<gpu::GpuChannelHost> GpuService::EstablishGpuChannel(
    shell::Connector* connector) {
  base::AutoLock auto_lock(lock_);
  if (gpu_channel_ && gpu_channel_->IsLost()) {
    gpu_channel_->DestroyChannel();
    gpu_channel_ = nullptr;
  }

  if (gpu_channel_)
    return gpu_channel_;

  mus::mojom::GpuServicePtr gpu_service;
  connector->ConnectToInterface("mojo:mus", &gpu_service);

  int client_id = 0;
  mojom::ChannelHandlePtr channel_handle;
  mojom::GpuInfoPtr gpu_info;
  {
    // TODO(penghuang): Remove the ScopedAllowSyncCall when HW rendering is
    // enabled in mus chrome.
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
    if (!gpu_service->EstablishGpuChannel(&client_id, &channel_handle,
                                          &gpu_info)) {
      DLOG(WARNING)
          << "Channel encountered error while establishing gpu channel.";
      return nullptr;
    }
  }

  // TODO(penghuang): Get the real gpu info from mus.
  gpu_channel_ = gpu::GpuChannelHost::Create(
      this, client_id, gpu::GPUInfo(), channel_handle.To<IPC::ChannelHandle>(),
      &shutdown_event_, gpu_memory_buffer_manager_.get());
  return gpu_channel_;
}

bool GpuService::IsMainThread() {
  return main_task_runner_->BelongsToCurrentThread();
}

scoped_refptr<base::SingleThreadTaskRunner>
GpuService::GetIOThreadTaskRunner() {
  return io_thread_.task_runner();
}

std::unique_ptr<base::SharedMemory> GpuService::AllocateSharedMemory(
    size_t size) {
  mojo::ScopedSharedBufferHandle handle =
      mojo::SharedBufferHandle::Create(size);
  if (!handle.is_valid())
    return nullptr;

  base::SharedMemoryHandle platform_handle;
  size_t shared_memory_size;
  bool readonly;
  MojoResult result = mojo::UnwrapSharedMemoryHandle(
      std::move(handle), &platform_handle, &shared_memory_size, &readonly);
  if (result != MOJO_RESULT_OK)
    return nullptr;
  DCHECK_EQ(shared_memory_size, size);

  return base::MakeUnique<base::SharedMemory>(platform_handle, readonly);
}

}  // namespace mus
