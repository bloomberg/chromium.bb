// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_gpu_service_holder.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif

namespace viz {

TestGpuServiceHolder::TestGpuServiceHolder()
    : gpu_thread_("GPUMainThread"), io_thread_("GPUIOThread") {
  CHECK(gpu_thread_.Start());
  CHECK(io_thread_.Start());

  base::WaitableEvent completion;
  gpu_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestGpuServiceHolder::InitializeOnGpuThread,
                                base::Unretained(this), &completion));
  completion.Wait();
}

TestGpuServiceHolder::~TestGpuServiceHolder() {
  // Ensure members created on GPU thread are destroyed there too.
  gpu_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestGpuServiceHolder::DeleteOnGpuThread,
                                base::Unretained(this)));
  gpu_thread_.Stop();
  io_thread_.Stop();
}

void TestGpuServiceHolder::InitializeOnGpuThread(
    base::WaitableEvent* completion) {
  DCHECK(gpu_thread_.task_runner()->BelongsToCurrentThread());
  auto* command_line = base::CommandLine::ForCurrentProcess();
  gpu::GpuPreferences gpu_preferences =
      gpu::gles2::ParseGpuPreferences(command_line);

  if (gpu_preferences.enable_vulkan) {
#if BUILDFLAG(ENABLE_VULKAN)
    vulkan_implementation_ = gpu::CreateVulkanImplementation();
    if (!vulkan_implementation_ ||
        !vulkan_implementation_->InitializeVulkanInstance(
            !gpu_preferences.disable_vulkan_surface)) {
      LOG(FATAL) << "Failed to create and initialize Vulkan implementation.";
    }
#else
    NOTREACHED();
#endif
  }
  // TODO(sgilhuly): Investigate why creating a GPUInfo and GpuFeatureInfo from
  // the command line causes the test SkiaOutputSurfaceImplTest.SubmitPaint to
  // fail on Android.
  gpu_service_ = std::make_unique<GpuServiceImpl>(
      gpu::GPUInfo(), /*watchdog_thread=*/nullptr, io_thread_.task_runner(),
      gpu::GpuFeatureInfo(), gpu_preferences,
      /*gpu_info_for_hardware_gpu=*/gpu::GPUInfo(),
      /*gpu_feature_info_for_hardware_gpu=*/gpu::GpuFeatureInfo(),
#if BUILDFLAG(ENABLE_VULKAN)
      vulkan_implementation_.get(),
#else
      /*vulkan_implementation=*/nullptr,
#endif
      /*exit_callback=*/base::DoNothing());

  // Use a disconnected mojo pointer, we don't need to receive any messages.
  mojom::GpuHostPtr gpu_host_proxy;
  mojo::MakeRequest(&gpu_host_proxy);
  gpu_service_->InitializeWithHost(
      std::move(gpu_host_proxy), gpu::GpuProcessActivityFlags(),
      gl::init::CreateOffscreenGLSurface(gfx::Size()),
      /*sync_point_manager=*/nullptr, /*shared_image_manager=*/nullptr,
      /*shutdown_event=*/nullptr);
  task_executor_ = std::make_unique<gpu::GpuInProcessThreadService>(
      gpu_thread_.task_runner(), gpu_service_->scheduler(),
      gpu_service_->sync_point_manager(), gpu_service_->mailbox_manager(),
      gpu_service_->share_group(),
      gpu_service_->gpu_channel_manager()
          ->default_offscreen_surface()
          ->GetFormat(),
      gpu_service_->gpu_feature_info(),
      gpu_service_->gpu_channel_manager()->gpu_preferences(),
      gpu_service_->shared_image_manager(),
      gpu_service_->gpu_channel_manager()->program_cache(),
      gpu_service_->GetContextState());

  completion->Signal();
}

void TestGpuServiceHolder::DeleteOnGpuThread() {
  task_executor_.reset();
  gpu_service_.reset();
}

}  // namespace viz
