// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_GPU_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_GPU_MOJO_MEDIA_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "media/mojo/services/mojo_media_client.h"

namespace media {

class MediaGpuChannelManager;

class GpuMojoMediaClient : public MojoMediaClient {
 public:
  // |media_gpu_channel_manager| must only be used on |gpu_task_runner|, which
  // is expected to be the GPU main thread task runner.
  GpuMojoMediaClient(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager);
  ~GpuMojoMediaClient() final;

  // MojoMediaClient implementation.
  std::unique_ptr<AudioDecoder> CreateAudioDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) final;
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      mojom::CommandBufferIdPtr command_buffer_id) final;
  std::unique_ptr<CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* interface_provider) final;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager_;

  DISALLOW_COPY_AND_ASSIGN(GpuMojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_GPU_MOJO_MEDIA_CLIENT_H_
