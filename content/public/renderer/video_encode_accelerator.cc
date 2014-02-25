// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/video_encode_accelerator.h"

#include "content/renderer/render_thread_impl.h"
#include "media/filters/gpu_video_accelerator_factories.h"

namespace content {

scoped_ptr<media::VideoEncodeAccelerator>
CreateVideoEncodeAccelerator(media::VideoEncodeAccelerator::Client* client) {
  scoped_ptr<media::VideoEncodeAccelerator> vea;

  scoped_refptr<media::GpuVideoAcceleratorFactories> gpu_factories =
      RenderThreadImpl::current()->GetGpuFactories();
  if (gpu_factories.get())
    vea = gpu_factories->CreateVideoEncodeAccelerator(client).Pass();

  return vea.Pass();
}

}  // namespace content
