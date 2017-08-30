// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_CHROME_CONTENT_GPU_CLIENT_H_
#define CHROME_GPU_CHROME_CONTENT_GPU_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "content/public/gpu/content_gpu_client.h"

#if defined(OS_CHROMEOS)
#include "components/arc/common/video_decode_accelerator.mojom.h"
#include "components/arc/common/video_encode_accelerator.mojom.h"
#include "gpu/command_buffer/service/gpu_preferences.h"

#endif

class ChromeContentGpuClient : public content::ContentGpuClient {
 public:
  ChromeContentGpuClient();
  ~ChromeContentGpuClient() override;

  // content::ContentGpuClient:
  void InitializeRegistry(service_manager::BinderRegistry* registry) override;
  void GpuServiceInitialized(
      const gpu::GpuPreferences& gpu_preferences) override;

 private:
#if defined(OS_CHROMEOS)
  void CreateArcVideoDecodeAccelerator(
      ::arc::mojom::VideoDecodeAcceleratorRequest request);

  void CreateArcVideoEncodeAccelerator(
      ::arc::mojom::VideoEncodeAcceleratorRequest request);
#endif

  // Used to profile process startup.
  base::StackSamplingProfiler stack_sampling_profiler_;

#if defined(OS_CHROMEOS)
  gpu::GpuPreferences gpu_preferences_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeContentGpuClient);
};

#endif  // CHROME_GPU_CHROME_CONTENT_GPU_CLIENT_H_
