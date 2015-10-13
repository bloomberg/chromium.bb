// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_process_control_impl.h"

#if defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "media/mojo/services/mojo_media_application.h"
#include "mojo/shell/static_application_loader.h"
#endif

namespace content {

GpuProcessControlImpl::GpuProcessControlImpl() {}

GpuProcessControlImpl::~GpuProcessControlImpl() {}

void GpuProcessControlImpl::RegisterApplicationLoaders(
    URLToLoaderMap* url_to_loader_map) {
#if defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  (*url_to_loader_map)[GURL("mojo:media")] =
      new mojo::shell::StaticApplicationLoader(
          base::Bind(&media::MojoMediaApplication::CreateApp),
          base::Bind(&base::DoNothing));
#endif
}

}  // namespace content
