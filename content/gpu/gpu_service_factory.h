// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_SERVICE_FACTORY_H_
#define CONTENT_GPU_GPU_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/child/service_factory.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "media/mojo/buildflags.h"

namespace media {
class MediaGpuChannelManager;
}

namespace content {

// Customization of ServiceFactory for the GPU process.
class GpuServiceFactory : public ServiceFactory {
 public:
  GpuServiceFactory(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      base::WeakPtr<media::MediaGpuChannelManager> media_gpu_channel_manager,
      media::AndroidOverlayMojoFactoryCB android_overlay_factory_cb);
  ~GpuServiceFactory() override;

  // ServiceFactory overrides:
  void RegisterServices(ServiceMap* services) override;

 private:
#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  // Task runner we were constructed on, and that |media_gpu_channel_manager_|
  // must be accessed from (the GPU main thread task runner). We expect
  // RegisterServices() to be called on this task runner as well, but the
  // implementation doesn't care.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<media::MediaGpuChannelManager> media_gpu_channel_manager_;
  media::AndroidOverlayMojoFactoryCB android_overlay_factory_cb_;
  gpu::GpuPreferences gpu_preferences_;
  gpu::GpuDriverBugWorkarounds gpu_workarounds_;
#endif

  DISALLOW_COPY_AND_ASSIGN(GpuServiceFactory);
};

}  // namespace content

#endif  // CONTENT_GPU_GPU_SERVICE_FACTORY_H_
