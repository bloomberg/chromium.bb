// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_process_control_impl.h"

#if defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/common/mojo/static_application_loader.h"
#include "media/mojo/services/mojo_media_application_factory.h"
#endif

namespace content {

GpuProcessControlImpl::GpuProcessControlImpl() {}

GpuProcessControlImpl::~GpuProcessControlImpl() {}

void GpuProcessControlImpl::RegisterApplicationLoaders(
    NameToLoaderMap* name_to_loader_map) {
#if defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  (*name_to_loader_map)["mojo:media"] =
    new StaticApplicationLoader(
    base::Bind(&media::CreateMojoMediaApplication),
          base::Bind(&base::DoNothing));
#endif
}

}  // namespace content
