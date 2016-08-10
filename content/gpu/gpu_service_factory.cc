// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_service_factory.h"

#if defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "media/mojo/services/mojo_media_application_factory.h"  // nogncheck
#endif

namespace content {

GpuServiceFactory::GpuServiceFactory() {}

GpuServiceFactory::~GpuServiceFactory() {}

void GpuServiceFactory::RegisterServices(ServiceMap* services) {
#if defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  MojoApplicationInfo service_info;
  service_info.application_factory =
      base::Bind(&media::CreateMojoMediaApplication);
  service_info.use_own_thread = true;
  services->insert(std::make_pair("mojo:media", service_info));
#endif
}

}  // namespace content
