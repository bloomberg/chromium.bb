// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/lib/gpu_service.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "components/mus/common/gpu_type_converters.h"
#include "components/mus/common/switches.h"
#include "components/mus/public/cpp/lib/gpu_memory_buffer_manager_mus.h"
#include "components/mus/public/interfaces/gpu_service.mojom.h"
#include "services/shell/public/cpp/connector.h"

namespace mus {

namespace {

void GpuChannelEstablishCallback(int* client_id_out,
                                 IPC::ChannelHandle* channel_handle_out,
                                 gpu::GPUInfo* gpu_info_out,
                                 int client_id,
                                 mus::mojom::ChannelHandlePtr channel_handle,
                                 mus::mojom::GpuInfoPtr gpu_info) {
  *client_id_out = client_id;
  *channel_handle_out = channel_handle.To<IPC::ChannelHandle>();
  // TODO(penghuang): Get the gpu info.
}
}

GpuService::GpuService()
    : main_message_loop_(base::MessageLoop::current()),
      shutdown_event_(false, false),
      io_thread_("GPUIOThread"),
      gpu_memory_buffer_manager_(new mus::GpuMemoryBufferManagerMus) {
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
  if (gpu_channel_ && gpu_channel_->IsLost()) {
    gpu_channel_->DestroyChannel();
    gpu_channel_ = nullptr;
  }

  if (gpu_channel_)
    return gpu_channel_;

  mus::mojom::GpuServicePtr gpu_service;
  connector->ConnectToInterface("mojo:mus", &gpu_service);

  int client_id = 0;
  IPC::ChannelHandle channel_handle;
  gpu::GPUInfo gpu_info;
  gpu_service->EstablishGpuChannel(base::Bind(
      &GpuChannelEstablishCallback, &client_id, &channel_handle, &gpu_info));
  if (!gpu_service.WaitForIncomingResponse()) {
    DLOG(WARNING)
        << "Channel encountered error while establishing gpu channel.";
    return nullptr;
  }
  gpu_channel_ = gpu::GpuChannelHost::Create(this, client_id, gpu_info,
                                             channel_handle, &shutdown_event_,
                                             gpu_memory_buffer_manager_.get());
  return gpu_channel_;
}

bool GpuService::IsMainThread() {
  return base::MessageLoop::current() == main_message_loop_;
}

scoped_refptr<base::SingleThreadTaskRunner>
GpuService::GetIOThreadTaskRunner() {
  return io_thread_.task_runner();
}

std::unique_ptr<base::SharedMemory> GpuService::AllocateSharedMemory(
    size_t size) {
  std::unique_ptr<base::SharedMemory> shm(new base::SharedMemory());
  if (!shm->CreateAnonymous(size))
    return std::unique_ptr<base::SharedMemory>();
  return shm;
}

}  // namespace mus
