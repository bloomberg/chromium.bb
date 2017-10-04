// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_service_factory.h"

#include <memory>

#include "base/threading/thread_task_runner_handle.h"
#include "services/shape_detection/public/interfaces/constants.mojom.h"
#include "services/shape_detection/shape_detection_service.h"

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#include "base/bind.h"
#include "media/mojo/services/media_service_factory.h"  // nogncheck
#endif

namespace content {

GpuServiceFactory::GpuServiceFactory(
    const gpu::GpuPreferences& gpu_preferences,
    base::WeakPtr<media::MediaGpuChannelManager> media_gpu_channel_manager,
    media::AndroidOverlayMojoFactoryCB android_overlay_factory_cb) {
#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  gpu_preferences_ = gpu_preferences;
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  media_gpu_channel_manager_ = std::move(media_gpu_channel_manager);
  android_overlay_factory_cb_ = std::move(android_overlay_factory_cb);
#endif
}

GpuServiceFactory::~GpuServiceFactory() {}

void GpuServiceFactory::RegisterServices(ServiceMap* services) {
#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  service_manager::EmbeddedServiceInfo info;
  info.factory =
      base::Bind(&media::CreateGpuMediaService, gpu_preferences_, task_runner_,
                 media_gpu_channel_manager_, android_overlay_factory_cb_);
  info.use_own_thread = true;
  services->insert(std::make_pair("media", info));
#endif

  service_manager::EmbeddedServiceInfo shape_detection_info;
  shape_detection_info.factory =
      base::Bind(&shape_detection::ShapeDetectionService::Create);
  services->insert(std::make_pair(shape_detection::mojom::kServiceName,
                                  shape_detection_info));
}

}  // namespace content
