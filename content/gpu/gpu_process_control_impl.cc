// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_process_control_impl.h"

#if defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/public/common/content_client.h"
#include "media/mojo/services/mojo_media_application_factory.h"
#if defined(OS_ANDROID)
#include "media/base/android/media_client_android.h"
#endif
#endif

namespace content {

GpuProcessControlImpl::GpuProcessControlImpl() {}

GpuProcessControlImpl::~GpuProcessControlImpl() {}

void GpuProcessControlImpl::RegisterApplicationFactories(
    ApplicationFactoryMap* factories) {
#if defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#if defined(OS_ANDROID)
  // Only set once per process instance.
  media::SetMediaClientAndroid(GetContentClient()->GetMediaClientAndroid());
#endif

  factories->insert(std::make_pair(
      "mojo:media", base::Bind(&media::CreateMojoMediaApplication)));
#endif
}

}  // namespace content
