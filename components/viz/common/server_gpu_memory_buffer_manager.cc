// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/server_gpu_memory_buffer_manager.h"

#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "services/ui/gpu/interfaces/gpu_service.mojom.h"

namespace viz {

ServerGpuMemoryBufferManager::ServerGpuMemoryBufferManager(
    ui::mojom::GpuService* gpu_service,
    int client_id)
    : gpu_service_(gpu_service),
      client_id_(client_id),
      native_configurations_(gpu::GetNativeGpuMemoryBufferConfigurations()),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      weak_factory_(this) {}

ServerGpuMemoryBufferManager::~ServerGpuMemoryBufferManager() {}

void ServerGpuMemoryBufferManager::AllocateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle,
    base::OnceCallback<void(const gfx::GpuMemoryBufferHandle&)> callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (gpu::GetNativeGpuMemoryBufferType() != gfx::EMPTY_BUFFER) {
    const bool is_native = native_configurations_.find(std::make_pair(
                               format, usage)) != native_configurations_.end();
    if (is_native) {
      pending_buffers_.insert(client_id);
      gpu_service_->CreateGpuMemoryBuffer(
          id, size, format, usage, client_id, surface_handle,
          base::Bind(&ServerGpuMemoryBufferManager::OnGpuMemoryBufferAllocated,
                     weak_factory_.GetWeakPtr(), client_id,
                     base::Passed(std::move(callback))));
      return;
    }
  }

  DCHECK(gpu::GpuMemoryBufferImplSharedMemory::IsUsageSupported(usage))
      << static_cast<int>(usage);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          gpu::GpuMemoryBufferImplSharedMemory::CreateGpuMemoryBuffer(id, size,
                                                                      format)));
}

std::unique_ptr<gfx::GpuMemoryBuffer>
ServerGpuMemoryBufferManager::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  gfx::GpuMemoryBufferId id(next_gpu_memory_id_++);
  gfx::GpuMemoryBufferHandle handle;
  base::WaitableEvent wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  DCHECK(!task_runner_->RunsTasksInCurrentSequence());
  auto reply_callback = base::BindOnce(
      [](gfx::GpuMemoryBufferHandle* handle, base::WaitableEvent* wait_event,
         const gfx::GpuMemoryBufferHandle& allocated_buffer_handle) {
        *handle = allocated_buffer_handle;
        wait_event->Signal();
      },
      &handle, &wait_event);
  // We block with a WaitableEvent until the callback is run. So using
  // base::Unretained() is safe here.
  auto allocate_callback =
      base::BindOnce(&ServerGpuMemoryBufferManager::AllocateGpuMemoryBuffer,
                     base::Unretained(this), id, client_id_, size, format,
                     usage, surface_handle, std::move(reply_callback));
  task_runner_->PostTask(FROM_HERE, std::move(allocate_callback));
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  wait_event.Wait();
  if (handle.is_null())
    return nullptr;
  return gpu::GpuMemoryBufferImpl::CreateFromHandle(
      handle, size, format, usage,
      base::Bind(&ServerGpuMemoryBufferManager::DestroyGpuMemoryBuffer,
                 weak_factory_.GetWeakPtr(), id, client_id_));
}

void ServerGpuMemoryBufferManager::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  static_cast<gpu::GpuMemoryBufferImpl*>(buffer)->set_destruction_sync_token(
      sync_token);
}

void ServerGpuMemoryBufferManager::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id,
    const gpu::SyncToken& sync_token) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (native_buffers_[client_id].erase(id))
    gpu_service_->DestroyGpuMemoryBuffer(id, client_id, sync_token);
}

void ServerGpuMemoryBufferManager::DestroyAllGpuMemoryBufferForClient(
    int client_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  for (gfx::GpuMemoryBufferId id : native_buffers_[client_id])
    gpu_service_->DestroyGpuMemoryBuffer(id, client_id, gpu::SyncToken());
  native_buffers_.erase(client_id);
  pending_buffers_.erase(client_id);
}

void ServerGpuMemoryBufferManager::OnGpuMemoryBufferAllocated(
    int client_id,
    base::OnceCallback<void(const gfx::GpuMemoryBufferHandle&)> callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (pending_buffers_.find(client_id) == pending_buffers_.end()) {
    // The client has been destroyed since the allocation request was made.
    if (!handle.is_null()) {
      gpu_service_->DestroyGpuMemoryBuffer(handle.id, client_id,
                                           gpu::SyncToken());
    }
    std::move(callback).Run(gfx::GpuMemoryBufferHandle());
    return;
  }
  if (!handle.is_null())
    native_buffers_[client_id].insert(handle.id);
  std::move(callback).Run(handle);
}

}  // namespace viz
