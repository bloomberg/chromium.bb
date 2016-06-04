// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_LIB_GPU_SERVICE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_LIB_GPU_SERVICE_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "gpu/ipc/client/gpu_channel_host.h"

namespace shell {
class Connector;
}

namespace mus {

class GpuMemoryBufferManagerMus;

class GpuService : public gpu::GpuChannelHostFactory {
 public:
  scoped_refptr<gpu::GpuChannelHost> EstablishGpuChannel(
      shell::Connector* connector);

  static bool UseChromeGpuCommandBuffer();
  static GpuService* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<GpuService>;

  GpuService();
  ~GpuService() override;

  // gpu::GpuChannelHostFactory overrides:
  bool IsMainThread() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner() override;
  std::unique_ptr<base::SharedMemory> AllocateSharedMemory(
      size_t size) override;

  base::MessageLoop* main_message_loop_;
  base::WaitableEvent shutdown_event_;
  base::Thread io_thread_;
  std::unique_ptr<GpuMemoryBufferManagerMus> gpu_memory_buffer_manager_;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_;

  DISALLOW_COPY_AND_ASSIGN(GpuService);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_LIB_GPU_SERVICE_CONNECTION_H_
